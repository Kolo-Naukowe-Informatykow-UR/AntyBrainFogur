/**
 * matter_app.cpp — Matter Air Quality Sensor integration
 *
 * Device type : Air Quality Sensor (0x002C)
 * Cluster     : CarbonDioxideConcentrationMeasurement (0x040D)
 *               MeasuredValue attribute — nullable float32, unit ppm
 *
 * Commissioning: BLE / NimBLE (ESP32-S3 supports BLE natively).
 *               QR code + manual code printed to serial on first boot.
 *               After commissioning BLE is released.
 *
 * DAC: example provider (development only — replace with factory-flashed
 *      credentials before production).
 */

#include "matter_app.h"
#include "ui.h"

#include <esp_log.h>
#include <nvs_flash.h>

/* esp-matter high-level API */
#include <esp_matter.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_cluster.h>
#include <esp_matter_feature.h>

/* CHIP platform and credentials */
#include <app/server/Server.h>
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <credentials/FabricTable.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ConfigurationManager.h>

static const char *TAG = "matter_app";

using namespace esp_matter;
using namespace chip::app::Clusters;

static uint16_t s_endpoint_id = 1;   /* populated by matter_app_init */

/* Pairing strings cached after stack start — 96 chars is plenty for both */
static char s_qr_code[96]     = {};
static char s_manual_code[32] = {};

const char *matter_app_get_qr_code(void)    { return s_qr_code[0]     ? s_qr_code     : NULL; }
const char *matter_app_get_manual_code(void){ return s_manual_code[0] ? s_manual_code : NULL; }

void matter_app_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset requested — wiping fabrics and rebooting");
    /* Schedules the reset on the CHIP task: clears chip_factory, chip_config,
     * chip_counters NVS partitions, then reboots.  Async — returns quickly.  */
    chip::Server::GetInstance().ScheduleFactoryReset();
}

bool matter_app_is_commissioned(void)
{
    /* IsFullyProvisioned() returns true once at least one fabric is stored.
     * Bool wrap avoids exposing C++ types over the C boundary.              */
    return chip::DeviceLayer::ConfigurationMgr().IsFullyProvisioned();
}

/* ── Attribute r/w callback ───────────────────────────────────────────────
 * Called by the Matter stack when a controller reads or writes an attribute.
 * We are read-only (sensor), so writes are silently accepted.               */
static esp_err_t app_attribute_cb(attribute::callback_type_t type,
                                  uint16_t endpoint_id,
                                  uint32_t cluster_id,
                                  uint32_t attribute_id,
                                  esp_matter_attr_val_t *val,
                                  void *priv_data)
{
    (void)type; (void)endpoint_id; (void)cluster_id;
    (void)attribute_id; (void)val; (void)priv_data;
    return ESP_OK;
}

/* ── Matter platform event callback ──────────────────────────────────────
 * Fired from the Matter task — UI updates must acquire the LVGL lock.       */
static void app_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event,
                         intptr_t arg)
{
    (void)arg;
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Matter commissioning complete — fabric added");
        ui_matter_set_commissioned(true);
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Matter fabric removed — back to un-commissioned state");
        ui_matter_set_commissioned(false);
        break;

    case chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionEstablished:
        ESP_LOGI(TAG, "BLE commissioning connection established");
        break;

    case chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionClosed:
        ESP_LOGI(TAG, "BLE commissioning connection closed");
        break;

    default:
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════════════════════════════════ */

esp_err_t matter_app_init(void)
{
    /* ── Matter node ───────────────────────────────────────────────────── */
    node::config_t node_cfg;
    node_t *root = node::create(&node_cfg, app_attribute_cb, NULL);
    if (!root) {
        ESP_LOGE(TAG, "node::create failed — check that esp-matter is linked");
        return ESP_FAIL;
    }

    /* ── Air Quality Sensor endpoint ───────────────────────────────────── */
    endpoint::air_quality_sensor::config_t aq_cfg;
    endpoint_t *ep = endpoint::air_quality_sensor::create(root, &aq_cfg,
                                                          ENDPOINT_FLAG_NONE,
                                                          NULL);
    if (!ep) {
        ESP_LOGE(TAG, "air_quality_sensor endpoint create failed");
        return ESP_FAIL;
    }
    s_endpoint_id = endpoint::get_id(ep);
    ESP_LOGI(TAG, "Air quality sensor endpoint id: %u", s_endpoint_id);

    /* ── CO2 concentration measurement cluster ─────────────────────────── */
    cluster::carbon_dioxide_concentration_measurement::config_t co2_cfg;
    cluster_t *co2_cl = cluster::carbon_dioxide_concentration_measurement::create(
        ep, &co2_cfg, CLUSTER_FLAG_SERVER);
    if (!co2_cl) {
        ESP_LOGE(TAG, "CarbonDioxideConcentrationMeasurement cluster create failed");
        return ESP_FAIL;
    }

    /* Add NumericMeasurement feature — this registers MeasuredValue (0x0000),
     * MinMeasuredValue, MaxMeasuredValue, and MeasurementUnit attributes.
     * Without this add(), attribute::update() for MeasuredValue returns 0x86. */
    {
        /* fully-qualified: esp_matter::cluster::carbon_dioxide... */
        cluster::carbon_dioxide_concentration_measurement::feature::numeric_measurement::config_t nm_cfg;
        /* defaults: measured_value = null, min/max = null,
         *           measurement_unit = 0 → ppm (per Matter spec 7.18.2) */
        if (cluster::carbon_dioxide_concentration_measurement::feature::numeric_measurement::add(
                co2_cl, &nm_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "numeric_measurement feature add failed");
            return ESP_FAIL;
        }
    }

    /* ── Device Attestation Credentials (example / dev only) ─────────── */
    chip::Credentials::SetDeviceAttestationCredentialsProvider(
        chip::Credentials::Examples::GetExampleDACProvider());

    /* ── Start Matter stack ────────────────────────────────────────────── *
     * Internally initialises:
     *   • CHIP platform manager                                             *
     *   • NimBLE BLE stack for commissioning                               *
     *   • Matter server + mDNS                                             *
     * QR code and manual pairing code are logged to serial.                */
    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_matter::start: %s", esp_err_to_name(err));
        return err;
    }

    /* ── Cache QR code + manual pairing code for the UI tile ────────────── */
    {
        /* GetQRCode / GetManualPairingCode write into a MutableCharSpan.
         * We use BLE rendezvous since that is our commissioning transport.    */
        chip::MutableCharSpan qr_span(s_qr_code, sizeof(s_qr_code) - 1);
        if (GetQRCode(qr_span,
                      chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE))
            == CHIP_NO_ERROR) {
            s_qr_code[qr_span.size()] = '\0';
            /* GetQRCode already includes the "MT:" URI prefix.  Don't add
             * it again — Matter scanners reject "MT:MT:..." payloads. */
            ESP_LOGI(TAG, "QR code: %s", s_qr_code);
        } else {
            ESP_LOGW(TAG, "GetQRCode failed (already commissioned?)");
        }

        chip::MutableCharSpan manual_span(s_manual_code, sizeof(s_manual_code) - 1);
        if (GetManualPairingCode(manual_span,
                                 chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE))
            == CHIP_NO_ERROR) {
            s_manual_code[manual_span.size()] = '\0';
            ESP_LOGI(TAG, "Manual code: %s", s_manual_code);
        }
    }

    ESP_LOGI(TAG, "Matter started — BLE commissioning active");
    ESP_LOGI(TAG, "Scan QR code or use manual code printed above to commission");
    return ESP_OK;
}

void matter_app_co2_update(int16_t ppm_co2)
{
    if (ppm_co2 <= 0) return;

    /* CarbonDioxideConcentrationMeasurement::MeasuredValue is a
     * nullable float32 (unit: ppm, per Matter spec 7.18.2).               */
    esp_matter_attr_val_t val;
    val.type  = ESP_MATTER_VAL_TYPE_NULLABLE_FLOAT;
    val.val.f = (float)ppm_co2;

    esp_err_t err = attribute::update(
        s_endpoint_id,
        CarbonDioxideConcentrationMeasurement::Id,
        CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
        &val);

    if (err != ESP_OK) {
        ESP_LOGD(TAG, "CO2 attribute::update failed: %s", esp_err_to_name(err));
    }
}
