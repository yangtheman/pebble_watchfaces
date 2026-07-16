/*
 * SHD "Division" watchface for Pebble Alloy.
 *
 * Orange tick ring, 7-segment time with seconds, date, ACTIVATED status,
 * step count (via FFI to the firmware health service), battery meter and
 * current temperature from Open-Meteo.
 */
import Poco from "commodetto/Poco";
import parseBMF from "commodetto/parseBMF";
import parseRLE from "commodetto/parseRLE";
import Battery from "embedded:sensor/Battery";
import Location from "embedded:sensor/Location";
import Message from "pebble/message";

const render = new Poco(screen);

function getFont(name, size) {
	const font = parseBMF(new Resource(`${name}-${size}.fnt`));
	font.bitmap = parseRLE(new Resource(`${name}-${size}-alpha.bm4`));
	return font;
}

const timeFont = getFont("DSEG7Classic-BoldItalic", 24);
const timeFontSmall = getFont("DSEG7Classic-BoldItalic", 20);
const dateFont = new render.Font("Gothic-Bold", 18);
const labelFont = new render.Font("Gothic-Bold", 14);
const statusFont = new render.Font("Gothic-Bold", 18);
const stepsFont = new render.Font("Gothic-Bold", 14);

const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);
const orange = render.makeColor(255, 85, 0);
const paleOrange = render.makeColor(255, 170, 85);

const DEG = Math.PI / 180;

let batteryPercent = -1;
let steps = -1;
let weather = null;

/* ---------- settings ---------- */

const DEFAULT_SETTINGS = {
	fahrenheit: false,
	usePhoneLocation: true,
	city: ""
};

function loadSettings() {
	try {
		const stored = localStorage.getItem("settings");
		if (stored)
			return { ...DEFAULT_SETTINGS, ...JSON.parse(stored) };
	} catch (e) {
	}
	return { ...DEFAULT_SETTINGS };
}

function saveSettings() {
	localStorage.setItem("settings", JSON.stringify(settings));
}

let settings = loadSettings();

new Message({
	keys: ["TemperatureUnit", "UsePhoneLocation", "CityName"],
	onReadable() {
		const msg = this.read();
		const unit = msg.get("TemperatureUnit");
		if (unit !== undefined)
			settings.fahrenheit = unit === 1;
		const usePhone = msg.get("UsePhoneLocation");
		if (usePhone !== undefined)
			settings.usePhoneLocation = usePhone === 1;
		const city = msg.get("CityName");
		if (city !== undefined)
			settings.city = String(city).trim();
		saveSettings();
		weather = null;
		refreshWeather();
		draw();
	}
});

/* ---------- data sources ---------- */

const battery = new Battery({
	onSample() {
		batteryPercent = this.sample().percent;
	}
});

function refreshSteps() {
	try {
		steps = Natives.getSteps();
	} catch (e) {
		steps = -1;
	}
}

function refreshWeather() {
	if (settings.usePhoneLocation) {
		try {
			new Location({
				onSample() {
					const sample = this.sample();
					this.close();
					fetchWeather(sample.latitude, sample.longitude);
				}
			});
		} catch (e) {
			console.log("location error: " + e);
		}
	} else if (settings.city) {
		geocodeCity(settings.city);
	}
}

// Resolve "City" or "City, State" to coordinates with Open-Meteo's free geocoder.
async function geocodeCity(city) {
	try {
		const parts = city.split(",").map(s => s.trim());
		const url = "http://geocoding-api.open-meteo.com/v1/search?count=10&name=" +
			encodeURIComponent(parts[0]);
		const response = await fetch(url);
		const data = await response.json();
		let results = data.results ?? [];
		if (parts[1]) {
			const state = parts[1].toLowerCase();
			const match = results.find(r =>
				(r.admin1 && r.admin1.toLowerCase() === state) ||
				(r.admin1_code && r.admin1_code.toLowerCase() === state));
			if (match)
				results = [match];
		}
		if (results.length)
			fetchWeather(results[0].latitude, results[0].longitude);
	} catch (e) {
		console.log("geocode error: " + e);
	}
}

async function fetchWeather(latitude, longitude) {
	try {
		let url = "http://api.open-meteo.com/v1/forecast?latitude=" + latitude +
			"&longitude=" + longitude + "&current=temperature_2m";
		if (settings.fahrenheit)
			url += "&temperature_unit=fahrenheit";
		const response = await fetch(url);
		const data = await response.json();
		weather = {
			temp: Math.round(data.current.temperature_2m),
			unit: settings.fahrenheit ? "F" : "C"
		};
		draw();
	} catch (e) {
		console.log("weather error: " + e);
	}
}

/* ---------- drawing helpers ---------- */

function radialLine(cx, cy, angleDeg, r0, r1, color, width) {
	const dx = Math.sin(angleDeg * DEG);
	const dy = -Math.cos(angleDeg * DEG);
	render.drawLine(
		Math.round(cx + dx * r0), Math.round(cy + dy * r0),
		Math.round(cx + dx * r1), Math.round(cy + dy * r1),
		color, width);
}

function drawRing(cx, cy, R) {
	// solid orange band
	render.drawCircle(orange, cx, cy, R);
	render.drawCircle(black, cx, cy, R - 13);

	// black ticks cut into the band: heavy at hours, light at minutes
	for (let i = 0; i < 60; i++) {
		const angle = i * 6;
		if (i % 5 === 0)
			radialLine(cx, cy, angle, R - 16, R + 2, black, 5);
		else
			radialLine(cx, cy, angle, R - 6, R + 2, black, 2);
	}
}

function drawBattery(x, y) {
	// body + terminal nub
	render.frameRoundRect(x - 12, y - 6, 22, 12, orange, 2);
	render.fillRectangle(orange, x + 11, y - 3, 3, 6);
	// charge segments
	if (batteryPercent >= 0) {
		const bars = Math.max(1, Math.round(batteryPercent / 25));
		for (let i = 0; i < bars; i++)
			render.fillRectangle(orange, x - 10 + i * 5, y - 4, 4, 8);
	}
}

function drawSun(x, y) {
	render.drawCircle(orange, x, y, 5);
	for (let i = 0; i < 8; i++)
		radialLine(x, y, i * 45, 7, 10, orange, 2);
}

function drawEmblem(cx, cy) {
	// orange roundel with dark core
	render.drawCircle(orange, cx, cy, 17);
	render.drawCircle(black, cx, cy, 14);

	// stylized SHD phoenix: body, head, fanned wings
	render.drawLine(cx, cy - 2, cx, cy + 9, white, 3);
	render.drawCircle(white, cx, cy - 5, 2);
	for (const s of [-1, 1]) {
		render.drawLine(cx + s * 2, cy + 1, cx + s * 13, cy - 8, white, 2);
		render.drawLine(cx + s * 2, cy + 4, cx + s * 11, cy - 2, white, 2);
		render.drawLine(cx + s * 2, cy + 7, cx + s * 8, cy + 3, white, 2);
	}
}

function centerText(text, font, color, cx, y) {
	render.drawText(text, font, color,
		Math.round(cx - render.getTextWidth(text, font) / 2), y);
}

/* ---------- main draw ---------- */

function draw() {
	const now = new Date();
	const cx = render.width >> 1;
	const cy = render.unobstructed.height >> 1;
	const R = Math.min(cx, cy) - 2;

	render.begin();
	render.fillRectangle(black, 0, 0, render.width, render.height);
	drawRing(cx, cy, R);

	// top row: battery, emblem, temperature
	const iconY = cy - 50;
	// equal clearance from the emblem edge (cx±17) to each icon's inner edge;
	// the battery glyph spans anchor-12..anchor+14, the sun spans anchor±10
	const gap = 9;
	const battX = cx - 17 - gap - 14;
	const sunX = cx + 17 + gap + 10;
	drawBattery(battX, iconY);
	drawEmblem(cx, cy - 62);
	drawSun(sunX, iconY);

	const battText = batteryPercent >= 0 ? `${batteryPercent}%` : "--";
	const tempText = weather ? `${weather.temp}${weather.unit}` : (settings.fahrenheit ? "--F" : "--C");
	centerText(battText, labelFont, white, battX + 1, iconY + 9);
	centerText(tempText, labelFont, white, sunX, iconY + 9);

	// 7-segment time with seconds (12h without leading zero, like the original)
	const hours = now.getHours();
	const h12 = hours % 12 === 0 ? 12 : hours % 12;
	const timeStr = `${h12}:${String(now.getMinutes()).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")}`;
	// widest chord available to the time block inside the ring, with margin
	const inner = R - 16;
	const maxTimeWidth = 2 * Math.sqrt(inner * inner - 26 * 26) - 10;
	const font = render.getTextWidth(timeStr, timeFont) <= maxTimeWidth ? timeFont : timeFontSmall;
	centerText(timeStr, font, orange, cx, cy - 8 - (font.height >> 1));

	// date, e.g. 06/17/19
	const dateStr = `${String(now.getMonth() + 1).padStart(2, "0")}/${String(now.getDate()).padStart(2, "0")}/${String(now.getFullYear() % 100).padStart(2, "0")}`;
	centerText(dateStr, dateFont, paleOrange, cx, cy + 10);

	centerText("ACTIVATED", statusFont, white, cx, cy + 32);

	const stepsText = steps >= 0 ? `${steps} STEPS` : "-- STEPS";
	centerText(stepsText, stepsFont, white, cx, cy + 56);

	render.end();
}

/* ---------- events ---------- */

watch.addEventListener("secondchange", e => {
	if (steps < 0 || e.date.getSeconds() % 30 === 0)
		refreshSteps();
	draw();
});
watch.addEventListener("hourchange", refreshWeather);
watch.addEventListener("resize", draw);
