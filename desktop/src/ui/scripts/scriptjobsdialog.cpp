// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptjobsdialog.h"

#include <algorithm>

#include <QApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "rpc/scriptjobsmanager.h"
#include "ansitext.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString stateText(const QString& state) {
            if (state == "queued"_L1) {
                return qApp->translate("tremotesf", "Queued");
            }
            if (state == "running"_L1) {
                return qApp->translate("tremotesf", "Running");
            }
            if (state == "finished"_L1) {
                return qApp->translate("tremotesf", "Finished");
            }
            if (state == "failed"_L1) {
                return qApp->translate("tremotesf", "Failed");
            }
            if (state == "canceled"_L1) {
                return qApp->translate("tremotesf", "Canceled");
            }
            if (state == "timed_out"_L1) {
                return qApp->translate("tremotesf", "Timed out");
            }
            return state;
        }

        QString resultText(const ScriptJob& job) {
            if (job.state == "finished"_L1) {
                return job.hasExitCode ? qApp->translate("tremotesf", "Exit code: %1").arg(job.exitCode)
                                       : qApp->translate("tremotesf", "Finished");
            }
            if (job.state == "canceled"_L1) {
                return qApp->translate("tremotesf", "Canceled");
            }
            if (job.state == "timed_out"_L1) {
                return qApp->translate("tremotesf", "Timed out");
            }
            if (job.state == "failed"_L1) {
                if (!job.error.isEmpty()) {
                    return job.error;
                }
                if (job.hasExitCode) {
                    return qApp->translate("tremotesf", "Exit code: %1").arg(job.exitCode);
                }
                return qApp->translate("tremotesf", "Failed");
            }
            return {};
        }

        QString startedText(const ScriptJob& job) {
            if (job.startedAt == 0) {
                return {};
            }
            return QLocale().toString(QDateTime::fromSecsSinceEpoch(job.startedAt), QLocale::ShortFormat);
        }

        QString durationText(const ScriptJob& job) {
            if (job.startedAt == 0) {
                return {};
            }
            const qint64 end = job.finishedAt != 0 ? job.finishedAt : QDateTime::currentSecsSinceEpoch();
            const qint64 seconds = std::max<qint64>(0, end - job.startedAt);
            if (seconds >= 3600) {
                return QStringLiteral("%1:%2:%3")
                    .arg(seconds / 3600)
                    .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                    .arg(seconds % 60, 2, 10, QLatin1Char('0'));
            }
            return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
        }

        QTextEdit* createOutputEdit(QWidget* parent) {
            auto* edit = new QTextEdit(parent);
            edit->setReadOnly(true);
            edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            edit->setPlaceholderText(qApp->translate("tremotesf", "No output"));
            return edit;
        }
    }

    ScriptJobsDialog::ScriptJobsDialog(ScriptJobsManager* manager, QWidget* parent)
        : QDialog(parent), mManager(manager) {
        setWindowTitle(qApp->translate("tremotesf", "Script jobs"));
        resize(900, 600);

        auto* layout = new QVBoxLayout(this);

        auto* splitter = new QSplitter(Qt::Vertical, this);

        mTable = new QTableWidget(splitter);
        mTable->setColumnCount(5);
        mTable->setHorizontalHeaderLabels(
            {qApp->translate("tremotesf", "Script"),
             qApp->translate("tremotesf", "State"),
             qApp->translate("tremotesf", "Started"),
             qApp->translate("tremotesf", "Duration"),
             qApp->translate("tremotesf", "Result")}
        );
        mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        mTable->setSelectionMode(QAbstractItemView::SingleSelection);
        mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mTable->verticalHeader()->setVisible(false);
        mTable->horizontalHeader()->setStretchLastSection(true);
        splitter->addWidget(mTable);

        auto* outputWidget = new QWidget(splitter);
        auto* outputLayout = new QVBoxLayout(outputWidget);
        outputLayout->setContentsMargins(0, 0, 0, 0);
        mStatusLabel = new QLabel(outputWidget);
        mStatusLabel->setWordWrap(true);
        outputLayout->addWidget(mStatusLabel);

        auto* tabs = new QTabWidget(outputWidget);
        mStdoutText = createOutputEdit(tabs);
        mStderrText = createOutputEdit(tabs);
        tabs->addTab(mStdoutText, qApp->translate("tremotesf", "stdout"));
        tabs->addTab(mStderrText, qApp->translate("tremotesf", "stderr"));
        outputLayout->addWidget(tabs);
        splitter->addWidget(outputWidget);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        layout->addWidget(splitter);

        auto* buttonsLayout = new QHBoxLayout();
        mCancelButton = new QPushButton(qApp->translate("tremotesf", "Cancel"), this);
        auto* clearButton = new QPushButton(qApp->translate("tremotesf", "Clear finished"), this);
        auto* closeButton = new QPushButton(qApp->translate("tremotesf", "Close"), this);
        buttonsLayout->addWidget(mCancelButton);
        buttonsLayout->addWidget(clearButton);
        buttonsLayout->addStretch();
        buttonsLayout->addWidget(closeButton);
        layout->addLayout(buttonsLayout);

        QObject::connect(mTable, &QTableWidget::itemSelectionChanged, this, &ScriptJobsDialog::updateDetails);
        QObject::connect(mCancelButton, &QPushButton::clicked, this, [this] {
            const auto jobId = selectedJobId();
            if (!jobId.isEmpty()) {
                mManager->cancelJob(jobId);
            }
        });
        QObject::connect(clearButton, &QPushButton::clicked, mManager, &ScriptJobsManager::clearFinished);
        QObject::connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

        QObject::connect(mManager, &ScriptJobsManager::jobAdded, this, [this](const QString& jobId) {
            refresh();
            showJob(jobId);
        });
        QObject::connect(mManager, &ScriptJobsManager::jobChanged, this, &ScriptJobsDialog::refresh);
        QObject::connect(mManager, &ScriptJobsManager::jobFinished, this, [this](const QString&) {
            refresh();
            if (!isActiveWindow()) {
                QApplication::alert(this);
            }
        });
        QObject::connect(mManager, &ScriptJobsManager::jobRemoved, this, &ScriptJobsDialog::refresh);

        refresh();
    }

    void ScriptJobsDialog::showJob(const QString& jobId) {
        for (int row = 0; row < mTable->rowCount(); ++row) {
            if (mTable->item(row, 0)->data(Qt::UserRole).toString() == jobId) {
                mTable->selectRow(row);
                break;
            }
        }
    }

    void ScriptJobsDialog::showEvent(QShowEvent* event) {
        QDialog::showEvent(event);
        // Jobs can keep running while this window is hidden, so always rebuild the
        // table and the selected job's output from the manager state on reopening.
        refresh();
    }

    void ScriptJobsDialog::refresh() {
        const auto selected = selectedJobId();
        const auto& jobs = mManager->jobs();

        mTable->setRowCount(static_cast<int>(jobs.size()));
        for (int row = 0; row < static_cast<int>(jobs.size()); ++row) {
            const auto& job = jobs.at(row);
            auto* scriptItem = new QTableWidgetItem(job.scriptName);
            scriptItem->setData(Qt::UserRole, job.id);
            mTable->setItem(row, 0, scriptItem);
            mTable->setItem(row, 1, new QTableWidgetItem(stateText(job.state)));
            mTable->setItem(row, 2, new QTableWidgetItem(startedText(job)));
            mTable->setItem(row, 3, new QTableWidgetItem(durationText(job)));
            mTable->setItem(row, 4, new QTableWidgetItem(resultText(job)));
        }
        mTable->resizeColumnsToContents();
        mTable->horizontalHeader()->setStretchLastSection(true);

        if (!selected.isEmpty()) {
            showJob(selected);
        }
        if (mTable->currentRow() < 0 && mTable->rowCount() > 0) {
            mTable->selectRow(0);
        }
        updateDetails();
    }

    void ScriptJobsDialog::updateDetails() {
        const auto jobId = selectedJobId();
        const auto* job = jobId.isEmpty() ? nullptr : mManager->job(jobId);
        if (!job) {
            mStatusLabel->clear();
            mStdoutText->clear();
            mStderrText->clear();
            mCancelButton->setEnabled(false);
            return;
        }

        QString status = qApp->translate("tremotesf", "State: %1").arg(stateText(job->state));
        if (job->selectionCount > 0) {
            status += u" | "_s + qApp->translate("tremotesf", "Selection: %1").arg(job->selectionCount);
        }
        if (!job->error.isEmpty()) {
            status += u" | "_s + job->error;
        }
        mStatusLabel->setText(status);

        QString stdoutText = job->stdoutText;
        if (job->stdoutTruncated) {
            stdoutText += u"\n\n"_s + qApp->translate("tremotesf", "[output truncated]");
        }
        QString stderrText = job->stderrText;
        if (job->stderrTruncated) {
            stderrText += u"\n\n"_s + qApp->translate("tremotesf", "[output truncated]");
        }
        mStdoutText->setHtml(scripts::ansiToHtml(stdoutText));
        mStderrText->setHtml(scripts::ansiToHtml(stderrText));
        mStdoutText->verticalScrollBar()->setValue(mStdoutText->verticalScrollBar()->maximum());
        mStderrText->verticalScrollBar()->setValue(mStderrText->verticalScrollBar()->maximum());
        mCancelButton->setEnabled(job->isActive());
    }

    QString ScriptJobsDialog::selectedJobId() const {
        const int row = mTable->currentRow();
        if (row < 0) {
            return {};
        }
        const auto* item = mTable->item(row, 0);
        return item ? item->data(Qt::UserRole).toString() : QString();
    }
}
