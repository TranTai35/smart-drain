/*
 * common.js - Ham dung chung cho ca ba trang.
 */

(function (global) {
    "use strict";

    var toastTimer = null;

    /** Hien thong bao ngan o goc phai tren. */
    function toast(message, isError) {
        var element = document.getElementById("toast");
        if (!element) { return; }

        element.textContent = message;
        element.className = "toast show " + (isError ? "err" : "ok");

        clearTimeout(toastTimer);
        toastTimer = setTimeout(function () {
            element.className = "toast";
        }, 3200);
    }

    /** Gui lenh xuong ESP32 qua Flask. Tra ve trang thai moi nhat kem theo. */
    function sendCommand(path, body) {
        return fetch(path, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body || {})
        })
            .then(function (response) {
                return response.json().then(function (data) {
                    return { ok: response.ok && data.ok, data: data };
                });
            })
            .then(function (result) {
                toast(result.data.message || "Đã gửi", !result.ok);
                return result;
            })
            .catch(function (error) {
                toast("Không gửi được lệnh: " + error.message, true);
                return { ok: false, data: {} };
            });
    }

    function getState() {
        return fetch("/api/state").then(function (r) { return r.json(); });
    }

    /** Cap nhat hai the ket noi tren thanh dieu huong. */
    function updateConnectionPills(state) {
        var esp = document.getElementById("esp-status");
        var mqtt = document.getElementById("mqtt-status");

        if (esp) {
            esp.textContent = "ESP32: " + (state.esp32_online ? "Online" : "Offline");
            esp.className = "pill " + (state.esp32_online ? "pill-on" : "pill-off");
        }

        if (mqtt) {
            mqtt.textContent = "MQTT: " + (state.mqtt_connected ? "Kết nối" : "Mất kết nối");
            mqtt.className = "pill " + (state.mqtt_connected ? "pill-on" : "pill-off");
        }
    }

    function setText(id, value) {
        var element = document.getElementById(id);
        if (element) { element.textContent = value; }
    }

    /** Hien "—" khi chua co du lieu, tranh hien "null" hay "undefined". */
    function orDash(value, suffix) {
        if (value === null || value === undefined || value === "") { return "—"; }
        return value + (suffix || "");
    }

    function formatTime(ms) {
        var d = new Date(ms);
        return String(d.getDate()).padStart(2, "0") + "/" +
               String(d.getMonth() + 1).padStart(2, "0") + " " +
               String(d.getHours()).padStart(2, "0") + ":" +
               String(d.getMinutes()).padStart(2, "0") + ":" +
               String(d.getSeconds()).padStart(2, "0");
    }

    /**
     * Lap lai mot viec theo chu ky, nhung dung setTimeout thay setInterval:
     * neu mot lan goi bi cham thi lan sau khong bi don cuc.
     */
    function poll(work, intervalMs) {
        function run() {
            Promise.resolve(work())
                .catch(function () { /* loi mang tam thoi thi bo qua */ })
                .then(function () { setTimeout(run, intervalMs); });
        }
        run();
    }

    global.SD = {
        toast: toast,
        sendCommand: sendCommand,
        getState: getState,
        updateConnectionPills: updateConnectionPills,
        setText: setText,
        orDash: orDash,
        formatTime: formatTime,
        poll: poll
    };
})(window);
