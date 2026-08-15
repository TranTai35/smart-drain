/*
 * dashboard.js - Trang tong quan.
 *
 * Cu moi giay goi /api/state va ve lai giao dien. Bieu do "10 phut gan nhat"
 * duoc dung tu chinh cac lan poll do, khong goi them API lich su.
 */

(function () {
    "use strict";

    var LIVE_WINDOW_MS = 10 * 60 * 1000;
    var livePoints = [];

    function badgeClass(level) {
        switch (level) {
            case "LOW": return "badge low";
            case "MEDIUM": return "badge medium";
            case "HIGH": return "badge high";
            case "DANGER": return "badge danger";
            default: return "badge";
        }
    }

    function renderTank(prefix, tank, dangerLimit) {
        var percent = tank.percent;
        var fill = document.getElementById(prefix + "-fill");
        var badge = document.getElementById(prefix + "-level");

        SD.setText(prefix + "-percent", SD.orDash(percent));

        if (fill) {
            fill.style.height = (typeof percent === "number" ? percent : 0) + "%";
            fill.classList.toggle(
                "danger",
                typeof percent === "number" && percent >= dangerLimit
            );
        }

        if (badge) {
            badge.textContent = tank.level_vi || "—";
            badge.className = badgeClass(tank.level);
        }
    }

    function renderAlert(state) {
        var alert = state.alert || {};
        var banner = document.getElementById("alert-banner");
        var active = alert.active && alert.code !== "NONE";

        if (banner) {
            banner.classList.toggle("hidden", !active);
            banner.classList.toggle("warning", alert.severity === "WARNING");
        }

        if (active) {
            SD.setText("alert-title", alert.message_vi || alert.message || alert.code);
            SD.setText(
                "alert-meta",
                "Mã: " + alert.code +
                " · Mức độ: " + alert.severity +
                (alert.muted ? " · Còi đang tắt tiếng" : "")
            );
        }

        var muteButton = document.getElementById("btn-mute");
        if (muteButton) {
            muteButton.textContent = alert.muted ? "Bật lại còi" : "Tắt còi";
        }

        SD.setText("alert-value", active ? alert.severity : "Bình thường");
        SD.setText("alert-sub", active ? (alert.message_vi || alert.code) : "Không có cảnh báo");

        var alertValue = document.getElementById("alert-value");
        if (alertValue) {
            alertValue.className = "stat-value " +
                (!active ? "on" : alert.severity === "DANGER" ? "bad" : "warn");
        }
    }

    function renderStats(state) {
        var pump = state.pump || {};
        var pumpOn = pump.state === "ON";

        SD.setText("pump-state", SD.orDash(pump.state));
        SD.setText(
            "pump-sub",
            pump.source
                ? pump.source_vi + (pumpOn ? " · " + pump.runtime + " giây" : "")
                : "—"
        );

        var pumpElement = document.getElementById("pump-state");
        if (pumpElement) {
            pumpElement.className = "stat-value " + (pumpOn ? "on" : "off");
        }

        SD.setText("mode-value", SD.orDash(state.mode));
        SD.setText(
            "mode-sub",
            state.mode === "AUTO" ? "ESP32 tự điều khiển" :
            state.mode === "MANUAL" ? "Người dùng điều khiển" : "—"
        );

        SD.setText("esp-value", state.esp32_online ? "Online" : "Offline");
        SD.setText("esp-sub", "Cập nhật: " + (state.last_seen_text || "—"));

        var espElement = document.getElementById("esp-value");
        if (espElement) {
            espElement.className = "stat-value " + (state.esp32_online ? "on" : "bad");
        }
    }

    function renderCapacity(state) {
        var output = state.output || {};
        var limit = (state.config && state.config.output_limit) || 80;

        if (typeof output.percent !== "number") {
            SD.setText("output-capacity", "—");
            return;
        }

        var remaining = Math.max(0, limit - output.percent);
        SD.setText(
            "output-capacity",
            remaining > 0 ? ("còn " + remaining + "% tới ngưỡng " + limit + "%")
                          : ("đã vượt ngưỡng " + limit + "%")
        );
    }

    var CONFIG_LABELS = {
        pump_start: "Ngưỡng bật bơm",
        pump_stop: "Ngưỡng tắt bơm",
        output_limit: "Giới hạn bể xả",
        max_runtime: "Thời gian bơm tối đa",
        drain_check: "Chu kỳ kiểm tra thoát nước",
        drain_min_drop: "Mức giảm tối thiểu"
    };

    var CONFIG_UNITS = {
        pump_start: "%", pump_stop: "%", output_limit: "%",
        max_runtime: " giây", drain_check: " giây", drain_min_drop: "%"
    };

    function renderConfig(state) {
        var grid = document.getElementById("config-grid");
        if (!grid) { return; }

        var config = state.config || {};
        var keys = Object.keys(CONFIG_LABELS).filter(function (key) {
            return config[key] !== undefined;
        });

        if (keys.length === 0) {
            grid.innerHTML = '<div class="muted">Đang chờ dữ liệu từ ESP32...</div>';
            return;
        }

        grid.innerHTML = keys.map(function (key) {
            return '<div class="config-item">' +
                   '<div class="k">' + CONFIG_LABELS[key] + "</div>" +
                   '<div class="v">' + config[key] + CONFIG_UNITS[key] + "</div>" +
                   "</div>";
        }).join("");
    }

    function pushLivePoint(state) {
        if (typeof state.input.percent !== "number") { return; }

        var now = Date.now();
        livePoints.push({
            ts: now,
            input_percent: state.input.percent,
            output_percent: state.output.percent
        });

        var cutoff = now - LIVE_WINDOW_MS;
        while (livePoints.length && livePoints[0].ts < cutoff) {
            livePoints.shift();
        }

        SmartDrainChart.draw(document.getElementById("live-chart"), livePoints);
    }

    function render(state) {
        SD.updateConnectionPills(state);

        var outputLimit = (state.config && state.config.output_limit) || 80;
        renderTank("input", state.input || {}, 90);
        renderTank("output", state.output || {}, outputLimit);

        SD.setText("input-adc", SD.orDash(state.input && state.input.adc));

        renderCapacity(state);
        renderStats(state);
        renderAlert(state);
        renderConfig(state);
        pushLivePoint(state);
    }

    var muteButton = document.getElementById("btn-mute");
    if (muteButton) {
        muteButton.addEventListener("click", function () {
            var turningOn = muteButton.textContent.indexOf("Bật lại") === 0;
            SD.sendCommand("/api/command/buzzer", {
                action: turningOn ? "UNMUTE" : "MUTE"
            }).then(function (result) {
                if (result.data && result.data.state) { render(result.data.state); }
            });
        });
    }

    // Ve lai bieu do khi doi kich thuoc cua so, neu khong bieu do se bi meo
    window.addEventListener("resize", function () {
        SmartDrainChart.draw(document.getElementById("live-chart"), livePoints);
    });

    SD.poll(function () {
        return SD.getState().then(render);
    }, 1000);
})();
