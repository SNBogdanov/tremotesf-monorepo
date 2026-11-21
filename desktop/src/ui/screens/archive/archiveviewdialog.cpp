// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archiveviewdialog.h"

#include <map>

#include <QBrush>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "formatutils.h"
#include "settings.h"
#include "ui/filetypeicons.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    ArchiveViewDialog::ArchiveViewDialog(
        QString torrentName, QString downloadDirectory, std::vector<ArchiveViewFile> files, QWidget* parent
    )
        : QDialog(parent) {
        setWindowTitle(torrentName.isEmpty() ? qApp->translate("tremotesf", "Torrent Contents") : torrentName);
        const auto* const settings = Settings::instance();
        if (const QByteArray geometry = settings->get_archiveViewDialogGeometry(); !geometry.isEmpty()) {
            restoreGeometry(geometry);
        } else {
            resize(760, 520);
        }

        auto* const layout = new QVBoxLayout(this);

        if (!downloadDirectory.isEmpty()) {
            //: %1 is download directory
            layout->addWidget(new QLabel(qApp->translate("tremotesf", "Save path: %1").arg(downloadDirectory), this));
        }

        qint64 totalSize = 0;
        for (const auto& file : files) {
            totalSize += file.size;
        }
        auto* const infoLabel = new QLabel(
            //: %n is number of files, %1 is total size
            qApp->translate("tremotesf", "%n file(s), %1", nullptr, static_cast<int>(files.size()))
                .arg(formatutils::formatByteSize(totalSize)),
            this
        );
        layout->addWidget(infoLabel);

        mTree = new QTreeWidget(this);
        mTree->setColumnCount(3);
        mTree->setHeaderLabels({
            //: Column name
            qApp->translate("tremotesf", "Name"),
            //: Column name
            qApp->translate("tremotesf", "Size"),
            //: Column name
            qApp->translate("tremotesf", "Wanted"),
        });
        mTree->setRootIsDecorated(true);
        mTree->setUniformRowHeights(true);
        mTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(mTree, 1);

        const QIcon folderIcon = filetypeicons::folderIcon();

        std::map<QString, QTreeWidgetItem*> directoryItems;
        std::map<QString, qint64> directorySizes;

        for (const auto& file : files) {
            const QStringList parts = file.path.split(QStringLiteral("/"));
            if (parts.isEmpty()) {
                continue;
            }

            QTreeWidgetItem* parentItem = nullptr;
            QString directoryPath;
            for (qsizetype i = 0; i < parts.size() - 1; ++i) {
                if (!directoryPath.isEmpty()) {
                    directoryPath += u'/';
                }
                directoryPath += parts[i];

                auto found = directoryItems.find(directoryPath);
                if (found == directoryItems.end()) {
                    auto* item = new QTreeWidgetItem();
                    item->setText(0, parts[i]);
                    item->setIcon(0, folderIcon);
                    if (parentItem) {
                        parentItem->addChild(item);
                    } else {
                        mTree->addTopLevelItem(item);
                    }
                    found = directoryItems.emplace(directoryPath, item).first;
                }
                parentItem = found->second;
                directorySizes[directoryPath] += file.size;
            }

            auto* fileItem = new QTreeWidgetItem();
            fileItem->setText(0, parts.back());
            fileItem->setText(1, formatutils::formatByteSize(file.size));
            fileItem->setIcon(0, filetypeicons::fileIcon(parts.back()));
            fileItem->setCheckState(2, file.wanted ? Qt::Checked : Qt::Unchecked);
            fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsUserCheckable);
            if (!file.wanted) {
                const QBrush gray(Qt::gray);
                fileItem->setForeground(0, gray);
                fileItem->setForeground(1, gray);
                fileItem->setForeground(2, gray);
            }
            if (parentItem) {
                parentItem->addChild(fileItem);
            } else {
                mTree->addTopLevelItem(fileItem);
            }
        }

        for (const auto& [path, item] : directoryItems) {
            item->setText(1, formatutils::formatByteSize(directorySizes[path]));
        }
        mTree->header()->setSectionResizeMode(QHeaderView::Interactive);
        if (const QByteArray headerState = settings->get_archiveViewHeaderState(); !headerState.isEmpty()) {
            mTree->header()->restoreState(headerState);
        } else {
            mTree->setColumnWidth(0, 520);
            mTree->setColumnWidth(1, 110);
            mTree->setColumnWidth(2, 90);
        }
        mTree->expandAll();

        QObject::connect(this, &QDialog::finished, this, [this] {
            auto* const settings = Settings::instance();
            settings->set_archiveViewDialogGeometry(saveGeometry());
            settings->set_archiveViewHeaderState(mTree->header()->saveState());
        });

        auto* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
        layout->addWidget(buttonBox);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &ArchiveViewDialog::reject);
    }
}
