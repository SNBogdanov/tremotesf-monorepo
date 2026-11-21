// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivemanagerdialog.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QVariant>
#include <QVBoxLayout>
#include <QtEndian>

#include "archiveutils.h"
#include "archiveviewdialog.h"
#include "archiverestoredialog.h"
#include "bencodeparser.h"
#include "formatutils.h"
#include "rpc/rpc.h"
#include "torrentfileparser.h"

using namespace Qt::StringLiterals;
using namespace std::string_view_literals;

namespace tremotesf {
    namespace {
        QString formatUnixDate(qint64 seconds) {
            if (seconds <= 0) {
                return {};
            }
            return formatutils::formatDateTime(QDateTime::fromSecsSinceEpoch(seconds), QLocale::ShortFormat);
        }

        QString metadataString(const QJsonObject& entry) {
            QStringList parts;
            if (entry.value("has_torrent"_L1).toBool()) {
                parts.append(qApp->translate("tremotesf", "torrent"));
            }
            if (entry.value("has_resume"_L1).toBool()) {
                parts.append(qApp->translate("tremotesf", "resume"));
            }
            if (entry.value("has_magnet"_L1).toBool()) {
                parts.append(qApp->translate("tremotesf", "magnet"));
            }
            if (parts.isEmpty()) {
                return qApp->translate("tremotesf", "none");
            }
            return parts.join(QStringLiteral(", "));
        }

        struct ArchiveBundleContents {
            QByteArray torrent;
            QByteArray resume;
        };

        /// Extracts .torrent and .resume files from ZIP bundle returned by torrent_archive/view
        std::optional<ArchiveBundleContents> extractFromBundle(const QByteArray& bundle, QString& error) {
            const auto readU16 = [&bundle](qsizetype offset) {
                return qFromLittleEndian<quint16>(bundle.constData() + offset);
            };
            const auto readU32 = [&bundle](qsizetype offset) {
                return qFromLittleEndian<quint32>(bundle.constData() + offset);
            };

            ArchiveBundleContents contents{};

            constexpr quint32 localFileHeaderSignature = 0x04034b50;
            qsizetype position = 0;
            while (position + 30 <= bundle.size()) {
                if (readU32(position) != localFileHeaderSignature) {
                    break;
                }
                const quint16 flags = readU16(position + 6);
                const quint16 compressionMethod = readU16(position + 8);
                const quint32 compressedSize = readU32(position + 18);
                const quint32 uncompressedSize = readU32(position + 22);
                const quint16 nameLength = readU16(position + 26);
                const quint16 extraLength = readU16(position + 28);

                if ((flags & 8) != 0) {
                    error = qApp->translate("tremotesf", "Unsupported ZIP data descriptor.");
                    return std::nullopt;
                }
                if (compressionMethod != 0 || compressedSize != uncompressedSize) {
                    error = qApp->translate("tremotesf", "Unsupported ZIP compression.");
                    return std::nullopt;
                }

                const qsizetype namePosition = position + 30;
                const qsizetype dataPosition = namePosition + nameLength + extraLength;
                if (dataPosition > bundle.size()
                    || dataPosition + static_cast<qsizetype>(compressedSize) > bundle.size()) {
                    error = qApp->translate("tremotesf", "Invalid ZIP entry.");
                    return std::nullopt;
                }

                const QByteArray entryName = bundle.mid(namePosition, nameLength);
                if (entryName.endsWith(".torrent")) {
                    contents.torrent = bundle.mid(dataPosition, compressedSize);
                } else if (entryName.endsWith(".resume")) {
                    contents.resume = bundle.mid(dataPosition, compressedSize);
                }
                if (!contents.torrent.isEmpty() && !contents.resume.isEmpty()) {
                    break;
                }
                position = dataPosition + compressedSize;
            }

            if (contents.torrent.isEmpty()) {
                return std::nullopt;
            }
            return contents;
        }

        std::optional<bencode::Value> parseBencodeBytes(const QByteArray& data, QString& error) {
            QTemporaryFile file;
            if (!file.open()) {
                error = file.errorString();
                return std::nullopt;
            }
            if (file.write(data) != data.size()) {
                error = file.errorString();
                return std::nullopt;
            }
            file.seek(0);
            try {
                return bencode::parse(file, [](const bencode::ByteArray&, qint64, qint64) {});
            } catch (const std::exception& e) {
                error = QString::fromUtf8(e.what());
                return std::nullopt;
            }
        }
    }

    ArchiveManagerDialog::ArchiveManagerDialog(Rpc* rpc, QWidget* parent) : QDialog(parent), mRpc(rpc) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Torrent Archive Manager"));
        resize(1000, 560);

        auto* const layout = new QVBoxLayout(this);

        auto* const topLayout = new QHBoxLayout();
        //: Archive manager dialog label
        topLayout->addWidget(new QLabel(qApp->translate("tremotesf", "Archive:"), this));
        mArchiveComboBox = new QComboBox(this);
        mArchiveComboBox->setMinimumContentsLength(24);
        mArchiveComboBox->setMaxVisibleItems(20);
        mArchiveComboBox->setStyleSheet("QComboBox { combobox-popup: 0; }");
        topLayout->addWidget(mArchiveComboBox, 1);

        auto* const refreshButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Refresh"),
            this
        );
        topLayout->addWidget(refreshButton);

        auto* const newArchiveButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "New archive..."),
            this
        );
        topLayout->addWidget(newArchiveButton);

        mDeleteArchiveButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Delete archive..."),
            this
        );
        topLayout->addWidget(mDeleteArchiveButton);

        mVerifyCheckBox = new QCheckBox(
            //: Checkbox
            qApp->translate("tremotesf", "Verify local data after restore"),
            this
        );
        topLayout->addWidget(mVerifyCheckBox);
        layout->addLayout(topLayout);

        mCountLabel = new QLabel(this);
        layout->addWidget(mCountLabel);

        mTable = new QTableWidget(this);
        mTable->setColumnCount(9);
        mTable->setHorizontalHeaderLabels({
            //: Archive manager column name
            qApp->translate("tremotesf", "Archive"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Name"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Size"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Added"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Archived"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Download folder"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Labels"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Metadata"),
            //: Archive manager column name
            qApp->translate("tremotesf", "Local data"),
        });
        mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        mTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mTable->setWordWrap(false);
        mTable->setTextElideMode(Qt::ElideRight);
        mTable->setSortingEnabled(false);
        mTable->verticalHeader()->setVisible(false);
        mTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        mTable->horizontalHeader()->setStretchLastSection(false);
        const std::array columnWidths{160, 320, 100, 130, 130, 220, 140, 150, 100};
        for (int i = 0; i < static_cast<int>(columnWidths.size()); ++i) {
            mTable->setColumnWidth(i, columnWidths[static_cast<size_t>(i)]);
        }
        layout->addWidget(mTable, 1);

        auto* const bottomLayout = new QHBoxLayout();
        mRestoreButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Restore..."),
            this
        );
        bottomLayout->addWidget(mRestoreButton);
        mViewButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "View..."),
            this
        );
        bottomLayout->addWidget(mViewButton);
        mDeleteEntryButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Delete..."),
            this
        );
        bottomLayout->addWidget(mDeleteEntryButton);
        bottomLayout->addStretch();
        mPreviousPageButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Previous"),
            this
        );
        bottomLayout->addWidget(mPreviousPageButton);
        mPageLabel = new QLabel(this);
        bottomLayout->addWidget(mPageLabel);
        mNextPageButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Next"),
            this
        );
        bottomLayout->addWidget(mNextPageButton);
        auto* const closeButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Close"),
            this
        );
        bottomLayout->addWidget(closeButton);
        layout->addLayout(bottomLayout);

        QObject::connect(refreshButton, &QPushButton::clicked, this, &ArchiveManagerDialog::refresh);
        QObject::connect(newArchiveButton, &QPushButton::clicked, this, &ArchiveManagerDialog::createArchive);
        QObject::connect(mDeleteArchiveButton, &QPushButton::clicked, this, &ArchiveManagerDialog::deleteArchive);
        QObject::connect(mRestoreButton, &QPushButton::clicked, this, &ArchiveManagerDialog::restoreSelected);
        QObject::connect(mViewButton, &QPushButton::clicked, this, &ArchiveManagerDialog::viewSelected);
        QObject::connect(mDeleteEntryButton, &QPushButton::clicked, this, &ArchiveManagerDialog::deleteSelected);
        QObject::connect(mPreviousPageButton, &QPushButton::clicked, this, [this] {
            mOffset = std::max(0, mOffset - pageSize);
            loadPage();
        });
        QObject::connect(mNextPageButton, &QPushButton::clicked, this, [this] {
            mOffset += pageSize;
            loadPage();
        });
        QObject::connect(closeButton, &QPushButton::clicked, this, &ArchiveManagerDialog::accept);
        QObject::connect(mArchiveComboBox, &QComboBox::currentIndexChanged, this, [this] {
            mOffset = 0;
            loadPage();
            updateControls();
        });
        QObject::connect(mTable, &QTableWidget::itemSelectionChanged, this, &ArchiveManagerDialog::updateControls);

        updateControls();
        loadArchives();
    }

    void ArchiveManagerDialog::refresh() { loadArchives(); }

    void ArchiveManagerDialog::loadArchives() {
        mRpc->requestArchive(QJsonObject{{u"action"_s, "list_archives"_L1}}, this, [this](ArchiveResponse response) {
            if (!response.success) {
                showError(qApp->translate("tremotesf", "Failed to load archive list."));
                return;
            }

            QString desired = mPendingArchiveSelection;
            if (desired.isEmpty()) {
                desired = currentArchive();
            }
            mPendingArchiveSelection.clear();

            const QSignalBlocker blocker(mArchiveComboBox);
            mArchiveComboBox->clear();
            //: Archive manager combo item
            mArchiveComboBox->addItem(qApp->translate("tremotesf", "All archives"), u"all"_s);
            const QJsonArray archives = response.arguments.value("archives"_L1).toArray();
            for (const auto& archive : archives) {
                if (archive.isString()) {
                    const QString name = archive.toString();
                    mArchiveComboBox->addItem(name, name);
                }
            }
            int index = mArchiveComboBox->findData(desired);
            if (index < 0) {
                index = 0;
            }
            mArchiveComboBox->setCurrentIndex(index);
            mOffset = 0;
            loadPage();
        });
    }

    void ArchiveManagerDialog::loadPage() {
        mRpc->requestArchive(
            QJsonObject{
                {u"action"_s, "list"_L1},
                {u"archive"_s, currentArchive()},
                {u"offset"_s, mOffset},
                {u"limit"_s, pageSize},
            },
            this,
            [this](ArchiveResponse response) {
                if (!response.success) {
                    showError(qApp->translate("tremotesf", "Failed to load archive entries."));
                    return;
                }

                mTotal = response.arguments.value("total"_L1).toInt();
                const QJsonArray entries = response.arguments.value("entries"_L1).toArray();
                mTable->setRowCount(0);
                for (const auto& entryValue : entries) {
                    const QJsonObject entry = entryValue.toObject();
                    const int row = mTable->rowCount();
                    mTable->insertRow(row);

                    const auto setText = [this, row](int column, const QString& text) {
                        mTable->setItem(row, column, new QTableWidgetItem(text));
                    };
                    setText(0, entry.value("archive"_L1).toString());
                    setText(1, entry.value("name"_L1).toString());
                    mTable->item(row, 1)->setToolTip(entry.value("name"_L1).toString());
                    setText(2, formatutils::formatByteSize(entry.value("totalSize"_L1).toVariant().toLongLong()));
                    setText(3, formatUnixDate(entry.value("addedDate"_L1).toVariant().toLongLong()));
                    setText(4, formatUnixDate(entry.value("archived_date"_L1).toVariant().toLongLong()));
                    setText(5, entry.value("download-dir"_L1).toString());

                    QStringList labels;
                    const QJsonArray labelsArray = entry.value("labels"_L1).toArray();
                    for (const auto& label : labelsArray) {
                        if (label.isString()) {
                            labels.append(label.toString());
                        }
                    }
                    setText(6, labels.join(", "_L1));

                    setText(7, metadataString(entry));
                    setText(
                        8,
                        entry.value("local_data_deleted"_L1).toBool()
                            //: Archive manager local data state
                            ? qApp->translate("tremotesf", "deleted")
                            //: Archive manager local data state
                            : qApp->translate("tremotesf", "present")
                    );

                    mTable->item(row, 0)->setData(
                        Qt::UserRole,
                        entry.value("archive"_L1).toString() + u'\t' + entry.value("hashString"_L1).toString()
                    );
                }

                mCountLabel->setText(
                    //: Archive manager entry count, %1 is number of entries
                    qApp->translate("tremotesf", "Entries: %1").arg(mTotal)
                );
                updateControls();
            }
        );
    }

    void ArchiveManagerDialog::createArchive() {
        bool ok = false;
        const QString name = QInputDialog::getText(
                                 this,
                                 //: Dialog title
                                 qApp->translate("tremotesf", "New archive"),
                                 //: Input dialog label
                                 qApp->translate("tremotesf", "New archive name:"),
                                 QLineEdit::Normal,
                                 {},
                                 &ok
        )
                                 .trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }
        if (!isValidArchiveName(name)) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "The archive name contains characters that are not allowed.")
            );
            return;
        }

        mRpc->requestArchive(
            QJsonObject{{u"action"_s, "create_archive"_L1}, {u"archive"_s, name}},
            this,
            [this, name](ArchiveResponse response) {
                if (!response.success) {
                    showError(qApp->translate("tremotesf", "Failed to create archive."));
                    return;
                }
                if (!response.arguments.value("created"_L1).toBool()) {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: Archive already exists, %1 is archive name
                        qApp->translate("tremotesf", "Archive \"%1\" already exists.").arg(name)
                    );
                    return;
                }
                mPendingArchiveSelection = name;
                loadArchives();
            }
        );
    }

    void ArchiveManagerDialog::deleteArchive() {
        const QString archive = currentArchive();
        if (archive == "all"_L1) {
            return;
        }
        if (mTotal > 0) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Warning message
                qApp->translate("tremotesf", "The archive is not empty. Remove or move its entries first.")
            );
            return;
        }
        if (QMessageBox::question(
                this,
                windowTitle(),
                //: Confirmation message, %1 is archive name
                qApp->translate("tremotesf", "Delete archive \"%1\"? Only empty archives can be deleted.").arg(archive)
            )
            != QMessageBox::Yes) {
            return;
        }

        mRpc->requestArchive(
            QJsonObject{{u"action"_s, "delete_archive"_L1}, {u"archive"_s, archive}},
            this,
            [this, archive](ArchiveResponse response) {
                if (!response.success) {
                    showError(qApp->translate("tremotesf", "Failed to delete archive."));
                    return;
                }
                if (response.arguments.value("deleted"_L1).toBool()) {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: Archive deleted, %1 is archive name
                        qApp->translate("tremotesf", "Archive \"%1\" was deleted.").arg(archive)
                    );
                } else {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: Archive not deleted, %1 is archive name
                        qApp->translate("tremotesf", "Archive \"%1\" was not deleted.").arg(archive)
                    );
                }
                loadArchives();
            }
        );
    }

    void ArchiveManagerDialog::restoreSelected() {
        const auto entries = selectedEntries();
        if (entries.isEmpty()) {
            QMessageBox::information(
                this,
                windowTitle(),
                //: Information message
                qApp->translate("tremotesf", "The archive does not contain any entries.")
            );
            return;
        }

        ArchiveRestoreDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        QString downloadDirectory;
        if (dialog.changeDownloadDirectory()) {
            downloadDirectory = dialog.downloadDirectory();
            if (downloadDirectory.isEmpty()) {
                QMessageBox::warning(
                    this,
                    windowTitle(),
                    //: Error message
                    qApp->translate("tremotesf", "Please select a download folder.")
                );
                return;
            }
        }

        QJsonArray items;
        for (const auto& [archive, hash] : entries) {
            items.append(QJsonObject{{u"archive"_s, archive}, {u"hash_string"_s, hash}});
        }
        QJsonObject arguments{
            {u"action"_s, "restore"_L1},
            {u"items"_s, items},
            {u"remove"_s, dialog.removeFromArchive()},
            {u"verify"_s, mVerifyCheckBox->isChecked()},
        };
        if (!downloadDirectory.isEmpty()) {
            arguments.insert(u"download_dir"_s, downloadDirectory);
        }

        mRpc->requestArchive(arguments, this, [this](ArchiveResponse response) {
            if (!response.success) {
                showError(qApp->translate("tremotesf", "Failed to restore torrents."));
                return;
            }
            const int restored = response.arguments.value("restored"_L1).toInt();
            QString errorsText;
            const QJsonArray errors = response.arguments.value("errors"_L1).toArray();
            for (const auto& errorValue : errors) {
                const QJsonObject error = errorValue.toObject();
                if (!errorsText.isEmpty()) {
                    errorsText += u'\n';
                }
                const QString archive = error.value("archive"_L1).toString();
                if (!archive.isEmpty()) {
                    errorsText += archive + QStringLiteral(": ");
                }
                QString hash = error.value("hash_string"_L1).toString();
                if (hash.isEmpty()) {
                    hash = error.value("hashString"_L1).toString();
                }
                errorsText += hash + QStringLiteral(": ") + error.value("error"_L1).toString();
            }

            if (errorsText.isEmpty()) {
                QMessageBox::information(
                    this,
                    windowTitle(),
                    //: %n is number of torrents
                    qApp->translate("tremotesf", "Restored %n torrent(s).", nullptr, restored)
                );
            } else {
                QMessageBox::warning(
                    this,
                    windowTitle(),
                    //: %n is number of torrents
                    qApp->translate("tremotesf", "Restored %n torrent(s), but some entries failed:", nullptr, restored)
                        + u'\n'
                        + errorsText
                );
            }
            loadPage();
        });
    }

    void ArchiveManagerDialog::viewSelected() {
        const auto entries = selectedEntries();
        if (entries.size() != 1) {
            QMessageBox::information(
                this,
                windowTitle(),
                //: Information message
                qApp->translate("tremotesf", "Select a single entry to view.")
            );
            return;
        }
        const QString archive = entries.first().first;
        const QString hash = entries.first().second;

        QString listDownloadDirectory;
        if (const auto rows = mTable->selectionModel()->selectedRows(); !rows.isEmpty()) {
            if (const auto* item = mTable->item(rows.first().row(), 5)) {
                listDownloadDirectory = item->text();
            }
        }

        mRpc->requestArchive(
            QJsonObject{
                {u"action"_s, "view"_L1},
                {u"archive"_s, archive},
                {u"hash_string"_s, hash},
            },
            this,
            [this, listDownloadDirectory](ArchiveResponse response) {
                if (!response.success) {
                    showError(qApp->translate("tremotesf", "Failed to load torrent metadata."));
                    return;
                }

                const QByteArray bundle =
                    QByteArray::fromBase64(response.arguments.value("data_base64"_L1).toString().toLatin1());
                if (bundle.isEmpty()) {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: Error message
                        qApp->translate("tremotesf", "The archive entry does not contain torrent metadata.")
                    );
                    return;
                }

                QString error;
                const auto contents = extractFromBundle(bundle, error);
                if (!contents.has_value()) {
                    if (error.isEmpty()) {
                        QMessageBox::information(
                            this,
                            windowTitle(),
                            //: Error message
                            qApp->translate("tremotesf", "The archive entry does not contain torrent metadata.")
                        );
                    } else {
                        QMessageBox::critical(
                            this,
                            windowTitle(),
                            //: Error message, %1 is error description
                            qApp->translate("tremotesf", "Could not read the torrent metadata: %1").arg(error)
                        );
                    }
                    return;
                }

                auto torrentValue = parseBencodeBytes(contents->torrent, error);
                if (!torrentValue.has_value()) {
                    QMessageBox::critical(
                        this,
                        windowTitle(),
                        //: Error message, %1 is error description
                        qApp->translate("tremotesf", "Could not read the torrent metadata: %1").arg(error)
                    );
                    return;
                }

                try {
                    TorrentMetainfoFile metainfo(std::move(*torrentValue), QString());

                    QString resumeName;
                    QString resumeDestination;
                    std::vector<QString> resumeFiles;
                    std::vector<bool> wanted;

                    if (!contents->resume.isEmpty()) {
                        QString resumeError;
                        if (auto resumeValue = parseBencodeBytes(contents->resume, resumeError);
                            resumeValue.has_value()) {
                            try {
                                auto root = std::move(*resumeValue).takeDictionary();
                                const auto takeString = [&root](std::string_view key) -> QString {
                                    const auto found = root.find(key);
                                    if (found == root.end()) {
                                        return {};
                                    }
                                    try {
                                        return std::move(found->second).takeString();
                                    } catch (const bencode::Error&) {
                                        return {};
                                    }
                                };
                                resumeName = takeString("name"sv);
                                resumeDestination = takeString("destination"sv);

                                if (const auto found = root.find("files"sv); found != root.end()) {
                                    try {
                                        for (auto& value : std::move(found->second).takeList()) {
                                            try {
                                                resumeFiles.push_back(std::move(value).takeString());
                                            } catch (const bencode::Error&) {
                                                resumeFiles.emplace_back();
                                            }
                                        }
                                    } catch (const bencode::Error&) {}
                                }
                                if (const auto found = root.find("dnd"sv); found != root.end()) {
                                    try {
                                        for (auto& value : std::move(found->second).takeList()) {
                                            try {
                                                wanted.push_back(std::move(value).takeInteger() == 0);
                                            } catch (const bencode::Error&) {
                                                wanted.push_back(true);
                                            }
                                        }
                                    } catch (const bencode::Error&) {}
                                }
                            } catch (const bencode::Error&) {
                                // Malformed resume file: fall back to torrent metadata
                            }
                        }
                    }

                    const QString originalRoot = metainfo.rootFileName;
                    const auto filePath = [&](QString path, std::size_t index) {
                        if (index < resumeFiles.size() && !resumeFiles[index].isEmpty()) {
                            return resumeFiles[index];
                        }
                        if (!resumeName.isEmpty() && !originalRoot.isEmpty()) {
                            if (path == originalRoot) {
                                return resumeName;
                            }
                            if (path.startsWith(originalRoot + u'/')) {
                                return resumeName + path.mid(originalRoot.size());
                            }
                        }
                        return path;
                    };

                    std::vector<ArchiveViewFile> files;
                    if (metainfo.isSingleFile()) {
                        files.push_back(
                            {filePath(originalRoot, 0), metainfo.singleFileSize(), wanted.empty() || wanted.front()}
                        );
                    } else {
                        std::size_t index = 0;
                        for (auto&& torrentFile : metainfo.files()) {
                            QStringList parts;
                            for (auto&& part : torrentFile.path()) {
                                parts.append(part);
                            }
                            const QString path = originalRoot + u'/' + parts.join(QStringLiteral("/"));
                            const bool fileWanted = index < wanted.size() ? wanted[index] : true;
                            files.push_back({filePath(path, index), torrentFile.size, fileWanted});
                            ++index;
                        }
                    }

                    if (files.empty()) {
                        QMessageBox::information(
                            this,
                            windowTitle(),
                            //: Information message
                            qApp->translate("tremotesf", "The torrent does not contain any files.")
                        );
                        return;
                    }

                    const QString displayName = resumeName.isEmpty() ? originalRoot : resumeName;
                    const QString downloadDirectory =
                        listDownloadDirectory.isEmpty() ? resumeDestination : listDownloadDirectory;
                    auto dialog = new ArchiveViewDialog(displayName, downloadDirectory, std::move(files), this);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->show();
                } catch (const bencode::Error& e) {
                    QMessageBox::critical(
                        this,
                        windowTitle(),
                        //: Error message, %1 is error description
                        qApp->translate("tremotesf", "Could not read the torrent metadata: %1")
                            .arg(QString::fromUtf8(e.what()))
                    );
                } catch (const std::exception& e) {
                    QMessageBox::critical(
                        this,
                        windowTitle(),
                        //: Error message, %1 is error description
                        qApp->translate("tremotesf", "Could not read the torrent metadata: %1")
                            .arg(QString::fromUtf8(e.what()))
                    );
                }
            }
        );
    }

    void ArchiveManagerDialog::deleteSelected() {
        const auto entries = selectedEntries();
        if (entries.isEmpty()) {
            QMessageBox::information(
                this,
                windowTitle(),
                //: Information message
                qApp->translate("tremotesf", "The archive does not contain any entries.")
            );
            return;
        }
        if (QMessageBox::question(
                this,
                windowTitle(),
                //: Confirmation message
                qApp->translate(
                    "tremotesf",
                    "Delete the selected torrent(s) from the archive? Downloaded data will not be deleted."
                )
            )
            != QMessageBox::Yes) {
            return;
        }
        deleteEntries(entries, 0, 0, {});
    }

    void ArchiveManagerDialog::deleteEntries(
        const QList<QPair<QString, QString>>& entries, int index, int deleted, const QStringList& errors
    ) {
        if (index >= entries.size()) {
            if (errors.isEmpty()) {
                QMessageBox::information(
                    this,
                    windowTitle(),
                    //: %n is number of entries
                    qApp->translate("tremotesf", "Deleted %n entry(ies) from the archive.", nullptr, deleted)
                );
            } else {
                QMessageBox::warning(
                    this,
                    windowTitle(),
                    //: %1 is number of deleted entries, %2 is number of selected entries
                    qApp->translate("tremotesf", "Deleted %1 of %2 entry(ies).").arg(deleted).arg(entries.size())
                        + u'\n'
                        + u'\n'
                        + errors.join(QStringLiteral("\n"))
                );
            }
            loadPage();
            return;
        }

        const auto& [archive, hash] = entries[index];
        mRpc->requestArchive(
            QJsonObject{
                {u"action"_s, "delete"_L1},
                {u"archive"_s, archive},
                {u"hash_string"_s, hash},
            },
            this,
            [this, entries, index, deleted, errors](ArchiveResponse response) {
                QStringList newErrors = errors;
                int newDeleted = deleted;
                if (response.success) {
                    ++newDeleted;
                } else {
                    newErrors.append(
                        entries[index].second
                        + QStringLiteral(": ")
                        //: Error message
                        + qApp->translate("tremotesf", "The archive entry could not be deleted.")
                    );
                }
                deleteEntries(entries, index + 1, newDeleted, newErrors);
            }
        );
    }

    QString ArchiveManagerDialog::currentArchive() const {
        const QVariant data = mArchiveComboBox->currentData();
        return data.isValid() ? data.toString() : "all"_L1;
    }

    QList<QPair<QString, QString>> ArchiveManagerDialog::selectedEntries() const {
        QList<QPair<QString, QString>> entries;
        if (!mTable->selectionModel()) {
            return entries;
        }
        const auto rows = mTable->selectionModel()->selectedRows();
        entries.reserve(rows.size());
        for (const QModelIndex& index : rows) {
            const QTableWidgetItem* item = mTable->item(index.row(), 0);
            if (!item) {
                continue;
            }
            const QString data = item->data(Qt::UserRole).toString();
            const int separator = data.indexOf(u'\t');
            if (separator < 0) {
                continue;
            }
            entries.append({data.left(separator), data.mid(separator + 1)});
        }
        return entries;
    }

    void ArchiveManagerDialog::updateControls() {
        const auto selected = selectedEntries();
        const bool hasSelection = !selected.isEmpty();
        mRestoreButton->setEnabled(hasSelection);
        mViewButton->setEnabled(selected.size() == 1);
        mDeleteEntryButton->setEnabled(hasSelection);
        mDeleteArchiveButton->setEnabled(currentArchive() != "all"_L1);

        const int pages = std::max(1, (mTotal + pageSize - 1) / pageSize);
        const int page = mOffset / pageSize + 1;
        mPageLabel->setText(
            //: Archive manager page, %1 is current page, %2 is total pages
            qApp->translate("tremotesf", "Page %1 of %2").arg(page).arg(pages)
        );
        mPreviousPageButton->setEnabled(mOffset > 0);
        mNextPageButton->setEnabled(mOffset + pageSize < mTotal);
    }

    void ArchiveManagerDialog::showError(const QString& message) {
        QMessageBox::critical(this, windowTitle(), message);
    }
}
