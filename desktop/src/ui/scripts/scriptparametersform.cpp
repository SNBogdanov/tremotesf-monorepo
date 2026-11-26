// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptparametersform.h"

#include <cmath>
#include <limits>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString hostName(const ScriptRunnerData& data, const QString& id) {
            if (const auto* host = data.externalHostById(id)) {
                return host->name;
            }
            return id;
        }

        QString jsonValueToText(const QJsonValue& value) {
            if (value.isString()) {
                return value.toString();
            }
            if (value.isBool()) {
                return value.toBool() ? "true"_L1 : "false"_L1;
            }
            if (value.isDouble()) {
                const double number = value.toDouble();
                if (std::floor(number) == number
                    && number >= static_cast<double>(std::numeric_limits<qint64>::min())
                    && number <= static_cast<double>(std::numeric_limits<qint64>::max())) {
                    return QString::number(static_cast<qint64>(number));
                }
                return QString::number(number, 'g', 17);
            }
            return {};
        }

        QWidget* createScalarEditor(const ScriptParameter& parameter, const ScriptRunnerData& data, QWidget* parent) {
            switch (parameter.type) {
            case ScriptParameterType::String:
            case ScriptParameterType::Integer:
            case ScriptParameterType::Number: {
                auto* edit = new QLineEdit(parent);
                if (parameter.defaultValue.has_value()) {
                    edit->setText(jsonValueToText(*parameter.defaultValue));
                }
                if (parameter.type != ScriptParameterType::String) {
                    edit->setPlaceholderText("0"_L1);
                }
                return edit;
            }
            case ScriptParameterType::Boolean: {
                auto* checkBox = new QCheckBox(parent);
                if (parameter.defaultValue.has_value()) {
                    checkBox->setChecked(parameter.defaultValue->toBool());
                }
                return checkBox;
            }
            case ScriptParameterType::Host: {
                auto* comboBox = new QComboBox(parent);
                for (const auto& hostId : parameter.hosts) {
                    comboBox->addItem(hostName(data, hostId), hostId);
                }
                if (parameter.defaultValue.has_value()) {
                    const auto index = comboBox->findData(parameter.defaultValue->toString());
                    if (index >= 0) {
                        comboBox->setCurrentIndex(index);
                    }
                }
                return comboBox;
            }
            }
            return nullptr;
        }

        QWidget* createArrayEditor(
            const ScriptParameter& parameter,
            const ScriptRunnerData& data,
            QWidget* parent,
            QListWidget** list,
            QLineEdit** lineEdit
        ) {
            auto* container = new QWidget(parent);
            auto* layout = new QVBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);

            auto* listWidget = new QListWidget(container);
            listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
            listWidget->setMinimumHeight(80);
            layout->addWidget(listWidget);
            *list = listWidget;

            if (parameter.type == ScriptParameterType::Host) {
                for (const auto& hostId : parameter.hosts) {
                    auto* item = new QListWidgetItem(hostName(data, hostId), listWidget);
                    item->setData(Qt::UserRole, hostId);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Unchecked);
                }
                if (parameter.defaultValue.has_value()) {
                    for (const auto& value : parameter.defaultValue->toArray()) {
                        const auto id = value.toString();
                        for (int i = 0; i < listWidget->count(); ++i) {
                            auto* item = listWidget->item(i);
                            if (item->data(Qt::UserRole).toString() == id) {
                                item->setCheckState(Qt::Checked);
                            }
                        }
                    }
                }
                return container;
            }

            auto* addRow = new QHBoxLayout();
            auto* edit = new QLineEdit(container);
            auto* addButton = new QPushButton(qApp->translate("tremotesf", "Add"), container);
            addRow->addWidget(edit);
            addRow->addWidget(addButton);
            layout->addLayout(addRow);
            *lineEdit = edit;

            auto* removeButton = new QPushButton(qApp->translate("tremotesf", "Remove"), container);
            layout->addWidget(removeButton);

            QObject::connect(addButton, &QPushButton::clicked, container, [listWidget, edit] {
                const auto text = edit->text();
                if (!text.isEmpty()) {
                    listWidget->addItem(text);
                    edit->clear();
                }
            });
            QObject::connect(edit, &QLineEdit::returnPressed, addButton, &QPushButton::click);
            QObject::connect(removeButton, &QPushButton::clicked, container, [listWidget] {
                delete listWidget->takeItem(listWidget->currentRow());
            });

            if (parameter.defaultValue.has_value()) {
                for (const auto& value : parameter.defaultValue->toArray()) {
                    listWidget->addItem(jsonValueToText(value));
                }
            }

            return container;
        }
    }

    ScriptParametersForm::ScriptParametersForm(
        const ScriptInfo& script, const ScriptRunnerData& runnerData, QWidget* parent, ScriptParameterStorage storage
    )
        : QWidget(parent), mStorage(storage) {
        auto* layout = new QFormLayout(this);
        layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        for (const auto& parameter : script.parameters) {
            if (parameter.storage != mStorage) {
                continue;
            }

            Input input{};
            input.parameter = parameter;
            if (parameter.array) {
                input.editor = createArrayEditor(parameter, runnerData, this, &input.list, &input.lineEdit);
            } else {
                input.editor = createScalarEditor(parameter, runnerData, this);
            }

            auto* label = new QLabel(parameter.name, this);
            if (parameter.required) {
                label->setText(parameter.name + " *"_L1);
            }
            label->setToolTip(parameter.description);
            input.editor->setToolTip(parameter.description);
            layout->addRow(label, input.editor);
            mInputs.push_back(std::move(input));
        }
    }

    void ScriptParametersForm::setValues(const QJsonObject& values) {
        for (auto& input : mInputs) {
            const auto& parameter = input.parameter;
            if (!values.contains(parameter.key)) {
                continue;
            }
            const auto value = values.value(parameter.key);

            if (parameter.array) {
                if (parameter.type == ScriptParameterType::Host) {
                    QStringList selected{};
                    for (const auto& item : value.toArray()) {
                        selected.push_back(item.toString());
                    }
                    for (int i = 0; i < input.list->count(); ++i) {
                        auto* item = input.list->item(i);
                        item->setCheckState(
                            selected.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked
                        );
                    }
                } else {
                    input.list->clear();
                    for (const auto& item : value.toArray()) {
                        input.list->addItem(jsonValueToText(item));
                    }
                }
                continue;
            }

            switch (parameter.type) {
            case ScriptParameterType::String:
            case ScriptParameterType::Integer:
            case ScriptParameterType::Number: {
                if (auto* edit = qobject_cast<QLineEdit*>(input.editor)) {
                    edit->setText(jsonValueToText(value));
                }
                break;
            }
            case ScriptParameterType::Boolean: {
                if (auto* checkBox = qobject_cast<QCheckBox*>(input.editor)) {
                    checkBox->setChecked(value.toBool());
                }
                break;
            }
            case ScriptParameterType::Host: {
                if (auto* comboBox = qobject_cast<QComboBox*>(input.editor)) {
                    const auto index = comboBox->findData(value.toString());
                    if (index >= 0) {
                        comboBox->setCurrentIndex(index);
                    }
                }
                break;
            }
            }
        }
    }

    std::optional<QJsonObject> ScriptParametersForm::parameters(QString* error) const {
        QJsonObject result{};

        const auto setError = [error](const QString& message) -> std::optional<QJsonObject> {
            if (error) {
                *error = message;
            }
            return std::nullopt;
        };

        const auto convertNumber = [&](const ScriptParameter& parameter, const QString& text, QJsonValue& value) {
            bool ok = false;
            if (parameter.type == ScriptParameterType::Integer) {
                const auto number = text.toLongLong(&ok);
                if (!ok) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" must be an integer").arg(parameter.name)
                    );
                }
                if (parameter.min.has_value() && static_cast<double>(number) < *parameter.min) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" is less than the minimum").arg(parameter.name)
                    );
                }
                if (parameter.max.has_value() && static_cast<double>(number) > *parameter.max) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" is greater than the maximum").arg(parameter.name)
                    );
                }
                value = number;
            } else {
                const auto number = text.toDouble(&ok);
                if (!ok || !std::isfinite(number)) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" must be a number").arg(parameter.name)
                    );
                }
                if (parameter.min.has_value() && number < *parameter.min) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" is less than the minimum").arg(parameter.name)
                    );
                }
                if (parameter.max.has_value() && number > *parameter.max) {
                    return setError(
                        qApp->translate("tremotesf", "Parameter \"%1\" is greater than the maximum").arg(parameter.name)
                    );
                }
                value = number;
            }
            return std::optional<QJsonObject>{result};
        };

        for (const auto& input : mInputs) {
            const auto& parameter = input.parameter;

            if (parameter.array) {
                QJsonArray array{};
                if (parameter.type == ScriptParameterType::Host) {
                    for (int i = 0; i < input.list->count(); ++i) {
                        const auto* item = input.list->item(i);
                        if (item->checkState() == Qt::Checked) {
                            array.append(item->data(Qt::UserRole).toString());
                        }
                    }
                } else {
                    for (int i = 0; i < input.list->count(); ++i) {
                        const auto text = input.list->item(i)->text();
                        QJsonValue value{};
                        if (parameter.type == ScriptParameterType::String) {
                            value = text;
                        } else if (parameter.type == ScriptParameterType::Boolean) {
                            if (text.compare("true"_L1, Qt::CaseInsensitive) == 0) {
                                value = true;
                            } else if (text.compare("false"_L1, Qt::CaseInsensitive) == 0) {
                                value = false;
                            } else {
                                return setError(
                                    qApp->translate("tremotesf", "Parameter \"%1\" must contain only true or false")
                                        .arg(parameter.name)
                                );
                            }
                        } else {
                            if (const auto optional = convertNumber(parameter, text, value); !optional.has_value()) {
                                return std::nullopt;
                            }
                        }
                        array.append(value);
                    }
                }
                if (array.isEmpty()) {
                    if (parameter.required) {
                        return setError(
                            qApp->translate("tremotesf", "Parameter \"%1\" is required").arg(parameter.name)
                        );
                    }
                    continue;
                }
                result.insert(parameter.key, array);
                continue;
            }

            switch (parameter.type) {
            case ScriptParameterType::String: {
                const auto* edit = qobject_cast<QLineEdit*>(input.editor);
                const auto text = edit->text();
                if (text.isEmpty() && !parameter.required) {
                    continue;
                }
                if (text.isEmpty() && parameter.required) {
                    return setError(qApp->translate("tremotesf", "Parameter \"%1\" is required").arg(parameter.name));
                }
                result.insert(parameter.key, text);
                break;
            }
            case ScriptParameterType::Integer:
            case ScriptParameterType::Number: {
                const auto* edit = qobject_cast<QLineEdit*>(input.editor);
                const auto text = edit->text().trimmed();
                if (text.isEmpty()) {
                    if (parameter.required) {
                        return setError(
                            qApp->translate("tremotesf", "Parameter \"%1\" is required").arg(parameter.name)
                        );
                    }
                    continue;
                }
                QJsonValue value{};
                if (const auto optional = convertNumber(parameter, text, value); !optional.has_value()) {
                    return std::nullopt;
                }
                result.insert(parameter.key, value);
                break;
            }
            case ScriptParameterType::Boolean: {
                const auto* checkBox = qobject_cast<QCheckBox*>(input.editor);
                result.insert(parameter.key, checkBox->isChecked());
                break;
            }
            case ScriptParameterType::Host: {
                const auto* comboBox = qobject_cast<QComboBox*>(input.editor);
                const auto id = comboBox->currentData().toString();
                if (id.isEmpty()) {
                    if (parameter.required) {
                        return setError(
                            qApp->translate("tremotesf", "Parameter \"%1\" is required").arg(parameter.name)
                        );
                    }
                    continue;
                }
                result.insert(parameter.key, id);
                break;
            }
            }
        }

        return result;
    }
}
