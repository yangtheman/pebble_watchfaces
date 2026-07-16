module.exports = [
  {
    "type": "heading",
    "defaultValue": "Division Watchface"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Weather"
      },
      {
        "type": "toggle",
        "messageKey": "TemperatureUnit",
        "label": "Use Fahrenheit",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "UsePhoneLocation",
        "label": "Use phone location",
        "description": "When off, weather uses the city below instead of GPS.",
        "defaultValue": true
      },
      {
        "type": "input",
        "messageKey": "CityName",
        "label": "City, State",
        "description": "Example: Austin, TX — used when phone location is off.",
        "defaultValue": "",
        "attributes": {
          "placeholder": "Austin, TX"
        }
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
