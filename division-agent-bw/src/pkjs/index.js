var Clay = require("@rebble/clay");
var clayConfig = require("./config");
var clay = new Clay(clayConfig);

var DEFAULT_SETTINGS = {
  TemperatureUnit: false,   // false = Celsius, true = Fahrenheit
  UsePhoneLocation: true,
  CityName: ""
};

function getSettings() {
  var settings = DEFAULT_SETTINGS;
  try {
    var stored = JSON.parse(localStorage.getItem("clay-settings")) || {};
    settings = {
      TemperatureUnit: "TemperatureUnit" in stored ? !!stored.TemperatureUnit : DEFAULT_SETTINGS.TemperatureUnit,
      UsePhoneLocation: "UsePhoneLocation" in stored ? !!stored.UsePhoneLocation : DEFAULT_SETTINGS.UsePhoneLocation,
      CityName: stored.CityName || ""
    };
  } catch (e) {}
  return settings;
}

function sendWeather(temp, unit) {
  Pebble.sendAppMessage({
    "WeatherTemp": Math.round(temp),
    "WeatherUnit": unit
  }, null, function(e) {
    console.log("weather send failed");
  });
}

function fetchWeather(lat, lon) {
  var settings = getSettings();
  var unit = settings.TemperatureUnit ? "F" : "C";
  var url = "https://api.open-meteo.com/v1/forecast?latitude=" + lat +
    "&longitude=" + lon + "&current=temperature_2m";
  if (settings.TemperatureUnit) url += "&temperature_unit=fahrenheit";

  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    try {
      var data = JSON.parse(this.responseText);
      sendWeather(data.current.temperature_2m, unit);
    } catch (e) {
      console.log("weather parse error: " + e);
    }
  };
  xhr.open("GET", url);
  xhr.send();
}

// Resolve "City" or "City, State" to coordinates with Open-Meteo's free geocoder.
function geocodeAndFetch(city) {
  var parts = city.split(",").map(function(s) { return s.trim(); });
  var url = "https://geocoding-api.open-meteo.com/v1/search?count=10&name=" +
    encodeURIComponent(parts[0]);

  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    try {
      var results = (JSON.parse(this.responseText).results) || [];
      if (parts[1]) {
        var state = parts[1].toLowerCase();
        for (var i = 0; i < results.length; i++) {
          var r = results[i];
          if ((r.admin1 && r.admin1.toLowerCase() === state) ||
              (r.admin1_code && r.admin1_code.toLowerCase() === state)) {
            results = [r];
            break;
          }
        }
      }
      if (results.length) fetchWeather(results[0].latitude, results[0].longitude);
    } catch (e) {
      console.log("geocode error: " + e);
    }
  };
  xhr.open("GET", url);
  xhr.send();
}

function refreshWeather() {
  var settings = getSettings();
  if (settings.UsePhoneLocation) {
    navigator.geolocation.getCurrentPosition(
      function(pos) { fetchWeather(pos.coords.latitude, pos.coords.longitude); },
      function(err) { console.log("location error: " + err.message); },
      { timeout: 15000, maximumAge: 60000 });
  } else if (settings.CityName) {
    geocodeAndFetch(settings.CityName);
  }
}

Pebble.addEventListener("ready", function() {
  refreshWeather();
  setInterval(refreshWeather, 30 * 60 * 1000);
});

Pebble.addEventListener("webviewclosed", function(e) {
  // Clay has already stored the new settings; re-fetch with them
  setTimeout(refreshWeather, 1000);
});
