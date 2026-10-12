#include "HidDescriptorPresentation.h"

#include <QString>
#include <QVariantList>

QVariantMap HidDescriptorPresentation::toVariant(const HidDescriptor::Result &result,
                                                std::size_t descriptorBytes) {
    QVariantList reports;
    QVariantList pages;
    QString error;
    if (result) {
        reports.reserve(static_cast<qsizetype>(result->reports.size()));
        pages.reserve(static_cast<qsizetype>(result->usagePages.size()));
        for (const auto &report : result->reports) {
            reports.append(QVariantMap{
                {"kind", QString::fromUtf8(HidDescriptor::name(report.kind))},
                {"id", report.id}, {"payloadBytes", report.payloadBytes}, {"wireBytes", report.wireBytes()}
            });
        }
        for (const auto page : result->usagePages) {
            pages.append(QString("0x%1").arg(page, 4, 16, QLatin1Char('0')));
        }
    } else {
        error = QString::fromUtf8(HidDescriptor::message(result.error().code));
    }
    QVariantMap info{{"valid", result.has_value()}, {"error", error},
                     {"descriptorBytes", QVariant::fromValue(static_cast<qulonglong>(descriptorBytes))},
                     {"usagePages", pages}, {"reports", reports}};
    if (!result) info.insert("errorOffset", QVariant::fromValue(static_cast<qulonglong>(result.error().offset)));
    return info;
}
