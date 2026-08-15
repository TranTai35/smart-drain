/*
 * history.js - Trang lich su: bieu do muc nuoc, nhat ky su kien, bo loc thoi gian.
 */

(function () {
    "use strict";

    // Khoang thoi gian dang xem. Hoac {hours: N}, hoac {from: "...", to: "..."}
    var currentRange = { hours: 1 };
    var currentType = "";

    function rangeQuery() {
        if (currentRange.hours) { return "hours=" + currentRange.hours; }

        var parts = [];
        if (currentRange.from) { parts.push("from=" + currentRange.from); }
        if (currentRange.to) { parts.push("to=" + currentRange.to); }
        return parts.join("&");
    }

    function loadChart() {
        return fetch("/api/history?limit=2000&" + rangeQuery())
            .then(function (r) { return r.json(); })
            .then(function (data) {
                var items = data.items || [];
                var empty = document.getElementById("chart-empty");

                if (empty) { empty.classList.toggle("hidden", items.length > 0); }
                SD.setText("history-count", items.length + " bản ghi");

                SmartDrainChart.draw(document.getElementById("history-chart"), items);
                window._historyPoints = items;
            })
            .catch(function () {
                SD.toast("Không tải được dữ liệu lịch sử", true);
            });
    }

    function eventRow(event) {
        var time = event.time_text || SD.formatTime(event.ts);
        var type = event.type || "";

        var ackCell = event.acked
            ? '<span class="acked">✓ Đã xác nhận</span>'
            : (type === "ALERT"
                ? '<button class="btn btn-small btn-ack" data-id="' + event.id + '">Xác nhận</button>'
                : '<span class="muted">—</span>');

        return "<tr>" +
            "<td>" + time + "</td>" +
            '<td><span class="tag ' + type + '">' + type + "</span></td>" +
            "<td>" + (event.message || "") + "</td>" +
            "<td>" + ackCell + "</td>" +
            "</tr>";
    }

    function loadEvents() {
        var query = "/api/events?limit=300&" + rangeQuery();
        if (currentType) { query += "&type=" + currentType; }

        return fetch(query)
            .then(function (r) { return r.json(); })
            .then(function (data) {
                var body = document.getElementById("event-body");
                if (!body) { return; }

                var items = data.items || [];
                if (items.length === 0) {
                    body.innerHTML = '<tr><td colspan="4" class="muted">' +
                        "Chưa có sự kiện nào trong khoảng thời gian này.</td></tr>";
                    return;
                }

                body.innerHTML = items.map(eventRow).join("");
                attachAckHandlers();
            })
            .catch(function () {
                SD.toast("Không tải được nhật ký sự kiện", true);
            });
    }

    function attachAckHandlers() {
        document.querySelectorAll(".btn-ack").forEach(function (button) {
            button.addEventListener("click", function () {
                button.disabled = true;

                fetch("/api/alert/ack", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ id: button.dataset.id })
                })
                    .then(function (r) { return r.json(); })
                    .then(function (data) {
                        SD.toast(data.message, !data.ok);
                        if (data.ok) { loadEvents(); } else { button.disabled = false; }
                    })
                    .catch(function () {
                        button.disabled = false;
                        SD.toast("Không xác nhận được", true);
                    });
            });
        });
    }

    function reload() {
        loadChart();
        loadEvents();
    }

    // --- nut khoang nhanh ---
    document.querySelectorAll(".range-btn").forEach(function (button) {
        button.addEventListener("click", function () {
            document.querySelectorAll(".range-btn").forEach(function (other) {
                other.classList.remove("active");
            });
            button.classList.add("active");

            currentRange = { hours: button.dataset.hours };
            document.getElementById("date-from").value = "";
            document.getElementById("date-to").value = "";
            reload();
        });
    });

    // --- loc theo ngay ---
    var applyButton = document.getElementById("btn-apply-range");
    if (applyButton) {
        applyButton.addEventListener("click", function () {
            var from = document.getElementById("date-from").value;
            var to = document.getElementById("date-to").value;

            if (!from && !to) {
                SD.toast("Chọn ít nhất một ngày", true);
                return;
            }

            if (from && to && from > to) {
                SD.toast("Ngày bắt đầu phải trước ngày kết thúc", true);
                return;
            }

            document.querySelectorAll(".range-btn").forEach(function (other) {
                other.classList.remove("active");
            });

            currentRange = { from: from, to: to };
            reload();
        });
    }

    // --- loc theo loai su kien ---
    var typeSelect = document.getElementById("event-filter");
    if (typeSelect) {
        typeSelect.addEventListener("change", function () {
            currentType = typeSelect.value;
            loadEvents();
        });
    }

    window.addEventListener("resize", function () {
        SmartDrainChart.draw(
            document.getElementById("history-chart"),
            window._historyPoints || []
        );
    });

    // Cap nhat cham hon dashboard: du lieu lich su khong doi nhanh,
    // va moi lan goi la mot lan doc Firebase
    reload();
    SD.poll(reload, 15000);
})();
