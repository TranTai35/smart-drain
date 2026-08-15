/*
 * control.js - Trang dieu khien.
 *
 * Nguyen tac quan trong: khi bam nut, giao dien KHONG tu doi trang thai.
 * No cho ESP32 publish lai pump/state roi moi ve lai theo du lieu that.
 * Nho vay neu lenh bi tu choi (be xa day, dang o AUTO...) man hinh se khong
 * hien sai so voi phan cung.
 */

(function () {
    "use strict";

    // Nguoi dung dang go so vao o nao thi khong ghi de o do
    var editingKey = null;

    function renderSummary(state) {
        var input = state.input || {};
        var output = state.output || {};
        var pump = state.pump || {};

        SD.setText("c-input", SD.orDash(input.percent, "%") +
            (input.level_vi ? " · " + input.level_vi : ""));
        SD.setText("c-output", SD.orDash(output.percent, "%") +
            (output.level_vi ? " · " + output.level_vi : ""));
        SD.setText("c-pump", SD.orDash(pump.state) +
            (pump.source_vi ? " · " + pump.source_vi : ""));
        SD.setText("c-mode", SD.orDash(state.mode));
        SD.setText("control-updated", "Cập nhật: " + (state.last_seen_text || "—"));

        var alertBadge = document.getElementById("c-alert");
        if (alertBadge) {
            var alert = state.alert || {};
            var active = alert.active && alert.code !== "NONE";
            alertBadge.textContent = active
                ? (alert.message_vi || alert.code)
                : "Không có cảnh báo";
            alertBadge.className = "badge " +
                (!active ? "low" : alert.severity === "DANGER" ? "danger" : "high");
        }
    }

    function renderButtons(state) {
        var online = state.esp32_online;
        var isManual = state.mode === "MANUAL";
        var output = state.output || {};
        var limit = (state.config && state.config.output_limit) || 80;
        var tankFull = typeof output.percent === "number" && output.percent >= limit;

        document.querySelectorAll(".btn-mode").forEach(function (button) {
            button.classList.toggle("active", button.dataset.mode === state.mode);
            button.disabled = !online;
        });

        var pumpOn = document.getElementById("btn-pump-on");
        var pumpOff = document.getElementById("btn-pump-off");

        if (pumpOn) { pumpOn.disabled = !online || !isManual || tankFull; }
        if (pumpOff) { pumpOff.disabled = !online; }

        var hint = document.getElementById("pump-hint");
        if (hint) {
            if (!online) {
                hint.textContent = "ESP32 đang offline, không gửi được lệnh.";
            } else if (!isManual) {
                hint.textContent = "Đang ở chế độ Auto. Chuyển sang Manual để điều khiển bơm bằng tay.";
            } else if (tankFull) {
                hint.textContent = "Bể xả đã đạt " + output.percent + "% (ngưỡng " +
                    limit + "%). Không thể bật bơm cho tới khi bể xả hạ xuống.";
            } else {
                hint.textContent = "Đang ở chế độ Manual, có thể bật hoặc tắt bơm. " +
                    "Lệnh tắt luôn được chấp nhận.";
            }
        }

        var muteButton = document.getElementById("btn-mute-page");
        var unmuteButton = document.getElementById("btn-unmute-page");
        var muted = state.alert && state.alert.muted;

        if (muteButton) { muteButton.disabled = !online || muted; }
        if (unmuteButton) { unmuteButton.disabled = !online || !muted; }
    }

    function renderConfigInputs(state) {
        var config = state.config || {};

        Object.keys(config).forEach(function (key) {
            if (key === editingKey) { return; }

            var input = document.getElementById("cfg-" + key);
            if (input && document.activeElement !== input) {
                input.value = config[key];
            }
        });
    }

    function render(state) {
        SD.updateConnectionPills(state);
        renderSummary(state);
        renderButtons(state);
        renderConfigInputs(state);
    }

    function afterCommand(result) {
        if (result.data && result.data.state) { render(result.data.state); }
    }

    // --- chon che do ---
    document.querySelectorAll(".btn-mode").forEach(function (button) {
        button.addEventListener("click", function () {
            SD.sendCommand("/api/command/mode", { mode: button.dataset.mode })
                .then(afterCommand);
        });
    });

    // --- bat / tat bom ---
    var pumpOnButton = document.getElementById("btn-pump-on");
    if (pumpOnButton) {
        pumpOnButton.addEventListener("click", function () {
            SD.sendCommand("/api/command/pump", { action: "ON" }).then(afterCommand);
        });
    }

    var pumpOffButton = document.getElementById("btn-pump-off");
    if (pumpOffButton) {
        pumpOffButton.addEventListener("click", function () {
            SD.sendCommand("/api/command/pump", { action: "OFF" }).then(afterCommand);
        });
    }

    // --- coi ---
    [["btn-mute-page", "MUTE"], ["btn-unmute-page", "UNMUTE"]].forEach(function (pair) {
        var button = document.getElementById(pair[0]);
        if (button) {
            button.addEventListener("click", function () {
                SD.sendCommand("/api/command/buzzer", { action: pair[1] })
                    .then(afterCommand);
            });
        }
    });

    // --- nguong ---
    document.querySelectorAll(".btn-cfg").forEach(function (button) {
        button.addEventListener("click", function () {
            var key = button.dataset.key;
            var input = document.getElementById("cfg-" + key);
            if (!input) { return; }

            editingKey = null;
            SD.sendCommand("/api/command/config", { key: key, value: input.value })
                .then(afterCommand);
        });
    });

    document.querySelectorAll(".config-input input").forEach(function (input) {
        input.addEventListener("focus", function () { editingKey = input.dataset.key; });
        input.addEventListener("blur", function () {
            if (editingKey === input.dataset.key) { editingKey = null; }
        });
        input.addEventListener("keydown", function (event) {
            if (event.key === "Enter") {
                document.querySelector('.btn-cfg[data-key="' + input.dataset.key + '"]').click();
            }
        });
    });

    SD.poll(function () {
        return SD.getState().then(render);
    }, 1000);
})();
