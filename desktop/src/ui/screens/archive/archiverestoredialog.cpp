// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archiverestoredialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace tremotesf {
    ArchiveRestoreDialog::ArchiveRestoreDialog(QWidget* parent) : QDialog(parent) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Restore Torrents"));
        resize(560, 220);

        auto* const layout = new QVBoxLayout(this);

        mChangePathCheckBox = new QCheckBox(
            //: Restore torrents dialog checkbox
            qApp->translate("tremotesf", "Change download folder"),
            this
        );
        layout->addWidget(mChangePathCheckBox);

        auto* const pathLayout = new QGridLayout();
        //: Restore torrents dialog label
        pathLayout->addWidget(new QLabel(qApp->translate("tremotesf", "New download folder"), this), 0, 0, 1, 2);
        mPathEdit = new QLineEdit(this);
        mPathEdit->setEnabled(false);
        pathLayout->addWidget(mPathEdit, 1, 0);
        mBrowseButton = new QPushButton(
            //: Button
            qApp->translate("tremotesf", "Browse..."),
            this
        );
        mBrowseButton->setEnabled(false);
        pathLayout->addWidget(mBrowseButton, 1, 1);
        layout->addLayout(pathLayout);

        mRemoveCheckBox = new QCheckBox(
            //: Restore torrents dialog checkbox
            qApp->translate("tremotesf", "Remove restored torrents from the archive"),
            this
        );
        mRemoveCheckBox->setChecked(true);
        layout->addWidget(mRemoveCheckBox);

        auto* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox
            ->button(QDialogButtonBox::Ok)
            //: Dialog confirmation button
            ->setText(qApp->translate("tremotesf", "Restore"));
        layout->addWidget(buttonBox);

        QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &ArchiveRestoreDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &ArchiveRestoreDialog::reject);
        QObject::connect(mChangePathCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            mPathEdit->setEnabled(checked);
            mBrowseButton->setEnabled(checked);
        });
        QObject::connect(mBrowseButton, &QPushButton::clicked, this, [this] {
            const QString directory = QFileDialog::getExistingDirectory(
                this,
                //: Dialog title
                qApp->translate("tremotesf", "Select Download Folder"),
                mPathEdit->text()
            );
            if (!directory.isEmpty()) {
                mPathEdit->setText(directory);
            }
        });
    }

    bool ArchiveRestoreDialog::changeDownloadDirectory() const { return mChangePathCheckBox->isChecked(); }

    QString ArchiveRestoreDialog::downloadDirectory() const { return mPathEdit->text().trimmed(); }

    bool ArchiveRestoreDialog::removeFromArchive() const { return mRemoveCheckBox->isChecked(); }
}
