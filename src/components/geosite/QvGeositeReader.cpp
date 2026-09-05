#include "QvGeositeReader.hpp"

// protozero: header-only protobuf reader. Lets Qv2ray read the v2ray
// geosite.dat / geoip.dat country-code lists WITHOUT linking libprotobuf, so
// protobuf/absl ABI churn stays out of the main executable entirely.
#include <protozero/pbf_reader.hpp>

#define QV_MODULE_NAME "GeositeReader"

namespace Qv2ray::components::geosite
{
    // Wire schema (both geosite.dat and geoip.dat share this shape):
    //
    //   GeoSiteList / GeoIPList { repeated GeoSite/GeoIP entry = 1; }
    //   GeoSite    / GeoIP     { string country_code = 1; ... }
    //
    // We only read each entry's `country_code` string (field 1). protozero skips
    // unknown fields automatically, so a v2ray geosite-format update that adds
    // new fields keeps this reader working (backward compatible).
    enum : protozero::pbf_tag_type
    {
        FIELD_ENTRY = 1,
        FIELD_COUNTRY_CODE = 1,
    };

    QMap<QString, QStringList> GeositeEntries;
    QStringList ReadGeoSiteFromFile(const QString &filepath)
    {
        if (GeositeEntries.contains(filepath))
        {
            return GeositeEntries.value(filepath);
        }
        else
        {
            QStringList list;
            QVLOG("Reading geosites from: " + filepath);

            QFile f(filepath);
            const bool opened = f.open(QFile::OpenModeFlag::ReadOnly);
            if (!opened)
            {
                QVLOG("File cannot be opened: " + filepath);
                return list;
            }

            const auto content = f.readAll();
            f.close();

            try
            {
                protozero::pbf_reader sites(content.constData(), content.size());
                while (sites.next(FIELD_ENTRY))
                {
                    protozero::pbf_reader entry = sites.get_message();
                    while (entry.next(FIELD_COUNTRY_CODE))
                    {
                        const auto cc = entry.get_string();
                        list << QString::fromUtf8(cc.data(), static_cast<int>(cc.size())).toLower();
                    }
                }
            }
            catch (const protozero::exception &e)
            {
                QVLOG("Failed to parse geosite data file: " + filepath + " (" + QString::fromUtf8(e.what()) + ")");
            }

            QVLOG("Loaded " + QSTRN(list.count()) + " geosite entries from data file.");
            list.sort();
            GeositeEntries[filepath] = list;
            return list;
        }
    }
} // namespace Qv2ray::components::geosite