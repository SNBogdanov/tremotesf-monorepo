// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivesavedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "archiveutils.h"
#include "rpc/rpc.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    ArchiveSaveDialog::ArchiveSaveDialog(Rpc* rpc, std::vector<int> torrentIds, QWidget* parent)
        : QDialog(parent), mRpc(rpc), mTorrentIds(std::move(torrentIds)) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Save to Archive"));

        auto* const layout = new QVBoxLayout(this);

        auto* const formLayout = new QFormLayout();
        mArchiveComboBox = new QComboBox(this);
        mArchiveComboBox->setEditable(true);
        mArchiveComboBox->setInsertPolicy(QComboBox::NoInsert);
        mArchiveComboBox->setMaxVisibleItems(20);
        //: Archive manager dialog label
        formLayout->addRow(qApp->translate("tremotesf", "Archive:"), mArchiveComboBox);
        layout->addLayout(formLayout);

        mOverwriteCheckBox = new QCheckBox(
            //: Save to archive dialog checkbox
            qApp->translate("tremotesf", "Overwrite existing entries"),
            this
        );
        layout->addWidget(mOverwriteCheckBox);

        mDeleteLocalDataCheckBox = new QCheckBox(
            //: Save to archive dialog checkbox
            qApp->translate("tremotesf", "Delete downloaded files after archiving"),
            this
        );
        layout->addWidget(mDeleteLocalDataCheckBox);

        mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        mButtonBox
            ->button(QDialogButtonBox::Ok)
            //: Dialog confirmation button
            ->setText(qApp->translate("tremotesf", "Save"));
        layout->addWidget(mButtonBox);

        QObject::connect(mButtonBox, &QDialogButtonBox::accepted, this, &ArchiveSaveDialog::save);
        QObject::connect(mButtonBox, &QDialogButtonBox::rejected, this, &ArchiveSaveDialog::reject);

        loadArchives();
    }

    void ArchiveSaveDialog::loadArchives() {
        mRpc->requestArchive(QJsonObject{{u"action"_s, "list_archives"_L1}}, this, [this](ArchiveResponse response) {
            if (!response.success) {
                return;
            }
            const QString current = mArchiveComboBox->currentText();
            const QJsonArray archives = response.arguments.value("archives"_L1).toArray();
            mArchiveComboBox->clear();
            for (const auto& archive : archives) {
                if (archive.isString()) {
                    mArchiveComboBox->addItem(archive.toString());
                }
            }
            mArchiveComboBox->setCurrentText(current);
        });
    }

    void ArchiveSaveDialog::save() {
        const QString archiveName = mArchiveComboBox->currentText().trimmed();
        if (archiveName.isEmpty()) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "Please enter an archive name.")
            );
            mArchiveComboBox->setFocus();
            return;
        }
        if (!isValidArchiveName(archiveName)) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "The archive name contains characters that are not allowed.")
            );
            mArchiveComboBox->setFocus();
            return;
        }
        if (mTorrentIds.empty()) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "There are no selected torrents to archive.")
            );
            return;
        }

        const auto count = static_cast<int>(mTorrentIds.size());
        if (mDeleteLocalDataCheckBox->isChecked()) {
            if (QMessageBox::question(
                    this,
                    windowTitle(),
                    //: Confirmation message, %n is number of torrents
                    qApp->translate(
                        "tremotesf",
                        "Delete the downloaded files of the %n selected torrent(s) after archiving?",
                        nullptr,
                        count
                    )
                )
                != QMessageBox::Yes) {
                return;
            }
        }

        QJsonArray ids;
        for (const int id : mTorrentIds) {
            ids.append(id);
        }

        mButtonBox->setEnabled(false);
        mRpc->requestArchive(
            QJsonObject{
                {u"action"_s, "save"_L1},
                {u"archive"_s, archiveName},
                {u"ids"_s, ids},
                {u"delete_local_data"_s, mDeleteLocalDataCheckBox->isChecked()},
                {u"overwrite"_s, mOverwriteCheckBox->isChecked()},
            },
            this,
            [this, archiveName](ArchiveResponse response) {
                mButtonBox->setEnabled(true);
                if (!response.success) {
                    QMessageBox::critical(
                        this,
                        windowTitle(),
                        //: Error message
                        qApp->translate("tremotesf", "Failed to save torrents to archive.")
                    );
                    return;
                }

                const int saved = response.arguments.value("saved"_L1).toInt();
                QString errorsText;
                const QJsonArray errors = response.arguments.value("errors"_L1).toArray();
                for (const auto& errorValue : errors) {
                    const QJsonObject error = errorValue.toObject();
                    if (!errorsText.isEmpty()) {
                        errorsText += u'\n';
                    }
                    errorsText += QStringLiteral("#%1: %2")
                                      .arg(error.value("id"_L1).toInt())
                                      .arg(error.value("error"_L1).toString());
                }

                if (errorsText.isEmpty()) {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: %n is number of torrents, %1 is archive name
                        qApp->translate("tremotesf", "Archived %n torrent(s) to archive \"%1\"", nullptr, saved)
                            .arg(archiveName)
                    );
                    accept();
                } else {
                    QMessageBox::warning(
                        this,
                        windowTitle(),
                        //: %n is number of torrents, %1 is archive name
                        qApp->translate(
                                "tremotesf",
                                "Archived %n torrent(s) to archive \"%1\", but some entries failed:",
                                nullptr,
                                saved
                        )
                                .arg(archiveName)
                            + u'\n'
                            + errorsText
                    );
                }
            }
        );
    }
}
