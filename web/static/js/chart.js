/*
 * chart.js - Bieu do duong ve bang Canvas thuan.
 *
 * Co y KHONG dung Chart.js hay thu vien nao khac: cac thu vien do phai tai tu
 * CDN, ma hom trinh dien rat co the khong co Internet (ESP32 chay bang hotspot
 * dien thoai). Bieu do tu ve thi luon chay duoc.
 */

(function (global) {
    "use strict";

    var COLORS = {
        input: "#38bdf8",
        output: "#a78bfa",
        grid: "rgba(148, 163, 184, 0.15)",
        axis: "rgba(148, 163, 184, 0.5)",
        text: "#94a3b8"
    };

    var PADDING = { top: 16, right: 16, bottom: 28, left: 38 };

    function formatClock(ms) {
        var d = new Date(ms);
        return String(d.getHours()).padStart(2, "0") + ":" +
               String(d.getMinutes()).padStart(2, "0");
    }

    /**
     * Ve bieu do muc nuoc hai be.
     *
     * @param {HTMLCanvasElement} canvas
     * @param {Array} points - [{ts, input_percent, output_percent}, ...] tang dan theo ts
     */
    function drawWaterChart(canvas, points) {
        if (!canvas) { return; }

        var ctx = canvas.getContext("2d");

        // Chieu cao goc phai NHO MOT LAN ROI GIU LAI.
        //
        // Gan canvas.height = ... cung chinh la ghi de thuoc tinh height cua the
        // <canvas>. Neu moi lan ve lai deu doc height roi nhan tiep voi
        // devicePixelRatio thi chieu cao tang theo cap so nhan: voi ty le 1.25
        // (Windows dat 125%), 220 -> 275 -> 344 -> 430... Dashboard ve lai moi
        // giay nen chi sau khoang mot phut canvas vuot gioi han cua trinh duyet,
        // trang dai vo tan va bieu do thanh o trang co bieu tuong mat buon.
        //
        // Vi vay lan dau doc xong thi cat vao dataset va nhung lan sau chi doc
        // tu do - dataset khong bi ham nay ghi de.
        var baseHeight = parseInt(canvas.dataset.baseHeight, 10);

        if (!baseHeight) {
            baseHeight = parseInt(canvas.getAttribute("height"), 10) || 220;
            canvas.dataset.baseHeight = baseHeight;
        }

        // Chan ty le lai: man hinh khai bao devicePixelRatio 3-4 se tao canvas
        // rat lon ma mat thuong khong thay dep hon
        var ratio = Math.min(global.devicePixelRatio || 1, 2);
        var cssWidth = canvas.clientWidth || 600;
        var cssHeight = baseHeight;

        canvas.width = Math.round(cssWidth * ratio);
        canvas.height = Math.round(cssHeight * ratio);
        canvas.style.height = cssHeight + "px";

        ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
        ctx.clearRect(0, 0, cssWidth, cssHeight);

        var plotWidth = cssWidth - PADDING.left - PADDING.right;
        var plotHeight = cssHeight - PADDING.top - PADDING.bottom;

        if (plotWidth <= 0 || plotHeight <= 0) { return; }

        // --- luoi ngang va nhan truc doc (0-100%) ---
        ctx.font = "11px 'Segoe UI', system-ui, sans-serif";
        ctx.textBaseline = "middle";
        ctx.textAlign = "right";

        for (var value = 0; value <= 100; value += 25) {
            var y = PADDING.top + plotHeight * (1 - value / 100);

            ctx.strokeStyle = COLORS.grid;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(PADDING.left, y);
            ctx.lineTo(PADDING.left + plotWidth, y);
            ctx.stroke();

            ctx.fillStyle = COLORS.text;
            ctx.fillText(value + "%", PADDING.left - 8, y);
        }

        if (!points || points.length === 0) { return; }

        var firstTs = points[0].ts;
        var lastTs = points[points.length - 1].ts;
        var span = lastTs - firstTs;
        if (span <= 0) { span = 1; }

        function toX(ts) {
            return PADDING.left + plotWidth * ((ts - firstTs) / span);
        }

        function toY(percent) {
            return PADDING.top + plotHeight * (1 - percent / 100);
        }

        // --- nhan truc thoi gian ---
        ctx.textAlign = "center";
        ctx.textBaseline = "top";
        ctx.fillStyle = COLORS.text;

        var labelCount = Math.max(2, Math.min(6, Math.floor(plotWidth / 90)));
        for (var i = 0; i < labelCount; i++) {
            var ts = firstTs + (span * i) / (labelCount - 1);
            var lx = toX(ts);
            // Ghim nhan dau va cuoi vao trong khung, tranh bi cat mat
            if (i === 0) { ctx.textAlign = "left"; }
            else if (i === labelCount - 1) { ctx.textAlign = "right"; }
            else { ctx.textAlign = "center"; }

            ctx.fillText(formatClock(ts), lx, PADDING.top + plotHeight + 8);
        }

        // --- hai duong du lieu ---
        drawSeries(ctx, points, "input_percent", COLORS.input, toX, toY,
                   PADDING.top + plotHeight);
        drawSeries(ctx, points, "output_percent", COLORS.output, toX, toY,
                   PADDING.top + plotHeight);

        // --- truc ---
        ctx.strokeStyle = COLORS.axis;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(PADDING.left, PADDING.top);
        ctx.lineTo(PADDING.left, PADDING.top + plotHeight);
        ctx.lineTo(PADDING.left + plotWidth, PADDING.top + plotHeight);
        ctx.stroke();
    }

    function drawSeries(ctx, points, field, color, toX, toY, baseline) {
        var valid = points.filter(function (p) {
            return typeof p[field] === "number";
        });

        if (valid.length === 0) { return; }

        // Vung to nhat duoi duong cho de doc
        var gradient = ctx.createLinearGradient(0, 0, 0, baseline);
        gradient.addColorStop(0, color + "40");
        gradient.addColorStop(1, color + "00");

        ctx.beginPath();
        ctx.moveTo(toX(valid[0].ts), baseline);
        valid.forEach(function (p) {
            ctx.lineTo(toX(p.ts), toY(p[field]));
        });
        ctx.lineTo(toX(valid[valid.length - 1].ts), baseline);
        ctx.closePath();
        ctx.fillStyle = gradient;
        ctx.fill();

        ctx.beginPath();
        valid.forEach(function (p, index) {
            var x = toX(p.ts);
            var y = toY(p[field]);
            if (index === 0) { ctx.moveTo(x, y); } else { ctx.lineTo(x, y); }
        });

        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.lineJoin = "round";
        ctx.lineCap = "round";
        ctx.stroke();
    }

    global.SmartDrainChart = { draw: drawWaterChart };
})(window);
