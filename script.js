
import {
    initializeApp
} from "https://www.gstatic.com/firebasejs/12.7.0/firebase-app.js";

import {
    getDatabase,
    ref,
    onValue
} from "https://www.gstatic.com/firebasejs/12.7.0/firebase-database.js";

// ==========================================
// RJ FIREBASE CONFIGURATION
// ==========================================

const firebaseConfig = {
    apiKey: "AIzaSyBeAHm65ky0p5mDpSMp6obm3FFcHIF-NCs",
    authDomain: "rjbeboso-5c489.firebaseapp.com",
    databaseURL: "https://rjbeboso-5c489-default-rtdb.europe-west1.firebasedatabase.app/",
    projectId: "rjbeboso-5c489",
    storageBucket: "rjbeboso-5c489.firebasestorage.app",
    messagingSenderId: "779243980452",
    appId: "1:779243980452:web:5d321d762e0d598d0b8831",
    measurementId: "G-FXFKRYPKKH"
};

// ==========================================
// INITIALIZE FIREBASE
// ==========================================

const firebaseApp = initializeApp(firebaseConfig);
const database = getDatabase(firebaseApp);
const dataRef = ref(database, "ESP32_Data");

// ==========================================
// GLOBAL VARIABLES
// ==========================================

let allSensorData = {};
let selectedDate = "";
let sensorChart = null;

// ==========================================
// DOM ELEMENTS
// ==========================================

const statusElement = document.getElementById("firebaseStatus");
const currentTemperature = document.getElementById("currentTemperature");
const currentHumidity = document.getElementById("currentHumidity");
const graphDate = document.getElementById("graphDate");
const historyBody = document.getElementById("historyTableBody");
const recordCount = document.getElementById("recordCount");
const toggleHistory = document.getElementById("toggleHistory");
const historyContent = document.getElementById("historyContent");

// ==========================================
// FIREBASE STATUS
// ==========================================

function setStatus(text, connected) {
    statusElement.textContent = "Firebase Status: " + text;

    statusElement.classList.toggle("status-connected", connected);
    statusElement.classList.toggle("status-error", !connected);
}

// ==========================================
// NUMBER HELPERS
// ==========================================

function toNumber(value) {
    const number = Number(value);
    return Number.isFinite(number) ? number : null;
}

function formatNumber(value) {
    return value === null || value === undefined
        ? "--"
        : Number(value).toFixed(1);
}

// ==========================================
// DATE LIST
// ==========================================

function getDateList(data) {
    return Object.keys(data || {})
        .filter(key =>
            data[key] &&
            typeof data[key] === "object"
        )
        .sort()
        .reverse();
}

// ==========================================
// POPULATE DATE SELECT
// ==========================================

function populateDateSelect() {
    const dates = getDateList(allSensorData);

    graphDate.innerHTML = "";

    if (dates.length === 0) {
        const option = document.createElement("option");
        option.value = "";
        option.textContent = "No dates available";
        graphDate.appendChild(option);
        return;
    }

    dates.forEach(date => {
        const option = document.createElement("option");
        option.value = date;
        option.textContent = date;
        graphDate.appendChild(option);
    });

    if (!dates.includes(selectedDate)) {
        selectedDate = dates[0];
    }

    graphDate.value = selectedDate;
}

// ==========================================
// GET LATEST READING
// ==========================================

function getLatestReading(data) {
    let latest = null;

    const dates = Object.keys(data || {}).sort();

    for (const date of dates) {
        const times = Object.keys(data[date] || {}).sort();

        for (const time of times) {
            const reading = data[date][time];

            if (!reading || typeof reading !== "object") {
                continue;
            }

            const temperature = toNumber(reading.temperature);
            const humidity = toNumber(reading.humidity);

            if (temperature === null && humidity === null) {
                continue;
            }

            latest = {
                date,
                time,
                temperature,
                humidity
            };
        }
    }

    return latest;
}

// ==========================================
// UPDATE CURRENT READINGS
// ==========================================

function updateCurrentReading() {
    const latest = getLatestReading(allSensorData);

    if (!latest) {
        currentTemperature.textContent = "-- °C";
        currentHumidity.textContent = "-- %";
        return;
    }

    currentTemperature.textContent =
        formatNumber(latest.temperature) + " °C";

    currentHumidity.textContent =
        formatNumber(latest.humidity) + " %";
}

// ==========================================
// GET READINGS FOR SELECTED DATE
// ==========================================

function getReadingsForDate(date) {
    const result = [];

    if (!date) return result;

    const dayData = allSensorData[date];

    if (!dayData || typeof dayData !== "object") {
        return result;
    }

    Object.keys(dayData).sort().forEach(time => {
        const reading = dayData[time];

        if (!reading || typeof reading !== "object") {
            return;
        }

        const temperature = toNumber(reading.temperature);
        const humidity = toNumber(reading.humidity);

        if (temperature === null && humidity === null) {
            return;
        }

        result.push({
            time,
            temperature,
            humidity
        });
    });

    return result;
}

// ==========================================
// UPDATE GRAPH
// ==========================================

function updateChart() {
    const readings = getReadingsForDate(graphDate.value);

    const canvas = document.getElementById("sensorChart");

    const labels = readings.map(item => item.time);

    const temperatures = readings.map(item => item.temperature);

    const humidities = readings.map(item => item.humidity);

    if (sensorChart) {
        sensorChart.destroy();
    }

    sensorChart = new Chart(canvas, {
        type: "line",

        data: {
            labels,

            datasets: [
                {
                    label: "Temperature (°C)",
                    data: temperatures,
                    yAxisID: "temperature",
                    borderColor: "#f87171",
                    backgroundColor: "rgba(248,113,113,0.12)",
                    tension: 0.3,
                    borderWidth: 3,
                    pointRadius: 3
                },

                {
                    label: "Humidity (%)",
                    data: humidities,
                    yAxisID: "humidity",
                    borderColor: "#38bdf8",
                    backgroundColor: "rgba(56,189,248,0.12)",
                    tension: 0.3,
                    borderWidth: 3,
                    pointRadius: 3
                }
            ]
        },

        options: {
            responsive: true,
            maintainAspectRatio: false,

            interaction: {
                mode: "index",
                intersect: false
            },

            plugins: {
                legend: {
                    labels: {
                        color: "#cbd5e1"
                    }
                }
            },

            scales: {
                x: {
                    ticks: {
                        color: "#94a3b8"
                    },
                    grid: {
                        color: "#334155"
                    }
                },

                temperature: {
                    type: "linear",
                    position: "left",

                    title: {
                        display: true,
                        text: "Temperature (°C)",
                        color: "#f87171"
                    },

                    ticks: {
                        color: "#f87171"
                    },

                    grid: {
                        color: "#334155"
                    }
                },

                humidity: {
                    type: "linear",
                    position: "right",

                    title: {
                        display: true,
                        text: "Humidity (%)",
                        color: "#38bdf8"
                    },

                    ticks: {
                        color: "#38bdf8"
                    },

                    grid: {
                        drawOnChartArea: false
                    }
                }
            }
        }
    });
}

// ==========================================
// UPDATE HISTORY
// ==========================================

function updateHistory() {
    const readings = getReadingsForDate(graphDate.value);

    historyBody.innerHTML = "";

    if (readings.length === 0) {
        historyBody.innerHTML =
            '<tr><td colspan="3">No sensor data available.</td></tr>';

        recordCount.textContent = "0 records";
        return;
    }

    readings.slice().reverse().forEach(reading => {
        const row = document.createElement("tr");

        const timeCell = document.createElement("td");
        const tempCell = document.createElement("td");
        const humCell = document.createElement("td");

        timeCell.textContent = reading.time;

        tempCell.textContent =
            formatNumber(reading.temperature) + " °C";

        humCell.textContent =
            formatNumber(reading.humidity) + " %";

        row.append(timeCell, tempCell, humCell);

        historyBody.appendChild(row);
    });

    recordCount.textContent =
        readings.length +
        (readings.length === 1 ? " record" : " records");
}

// ==========================================
// UPDATE DASHBOARD
// ==========================================

function updateDashboard() {
    const dates = getDateList(allSensorData);

    if (dates.length === 0) {
        currentTemperature.textContent = "-- °C";
        currentHumidity.textContent = "-- %";

        graphDate.innerHTML =
            '<option value="">No dates available</option>';

        historyBody.innerHTML =
            '<tr><td colspan="3">No sensor data available.</td></tr>';

        recordCount.textContent = "0 records";

        if (sensorChart) {
            sensorChart.destroy();
            sensorChart = null;
        }

        return;
    }

    populateDateSelect();
    updateCurrentReading();
    updateChart();
    updateHistory();
}

// ==========================================
// FIREBASE LIVE LISTENER
// ==========================================

console.log("Connecting to RJ Firebase...");

setStatus("Connecting...", false);

onValue(
    dataRef,

    snapshot => {
        allSensorData = snapshot.val() || {};

        setStatus("Connected", true);

        updateDashboard();

        console.log("RJ Firebase data updated.");
    },

    error => {
        console.error("Firebase error:", error);
        setStatus("Connection Error", false);
    }
);

// ==========================================
// DATE FILTER
// ==========================================

graphDate.addEventListener("change", function () {
    selectedDate = this.value;

    updateChart();
    updateHistory();
});

// ==========================================
// SHOW / HIDE HISTORY
// ==========================================

toggleHistory.addEventListener("click", function () {
    const hidden = historyContent.classList.toggle("hidden");

    toggleHistory.textContent =
        hidden ? "Show History" : "Hide History";

    if (!hidden) {
        updateHistory();
    }
});

// ==========================================
// RELAY DISPLAY
// ==========================================

function updateRelayDisplay(relay, isOn) {
    const status = document.getElementById(
        relay === 1 ? "led1Status" : "led2Status"
    );

    const button = document.getElementById(
        relay === 1 ? "led1Button" : "led2Button"
    );

    status.textContent = isOn ? "ON" : "OFF";

    status.className = "led-status " + (isOn ? "on" : "off");

    button.textContent = isOn ? "Turn OFF" : "Turn ON";
}

// ==========================================
// REFRESH RELAY STATUS
// ==========================================

async function refreshRelayStatus() {
    try {
        const response = await fetch("/relay-status", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("Relay status unavailable");
        }

        const data = await response.json();

        updateRelayDisplay(1, data.LED1);
        updateRelayDisplay(2, data.LED2);

    } catch (error) {
        console.error("Relay status error:", error);
    }
}

// ==========================================
// RELAY CONTROL
// ==========================================

async function toggleRelay(relay) {
    const button = document.getElementById(
        relay === 1 ? "led1Button" : "led2Button"
    );

    const message = document.getElementById("relayMessage");

    button.disabled = true;

    message.textContent = "Sending command...";

    try {
        const currentResponse = await fetch("/relay-status", {
            cache: "no-store"
        });

        if (!currentResponse.ok) {
            throw new Error("Cannot read relay status");
        }

        const current = await currentResponse.json();

        const currentState =
            relay === 1 ? current.LED1 : current.LED2;

        const nextState = currentState ? "off" : "on";

        const response = await fetch(
            `/relay?relay=${relay}&state=${nextState}`,
            { cache: "no-store" }
        );

        if (!response.ok) {
            throw new Error("Relay command failed");
        }

        const result = await response.json();

        if (!result.success) {
            throw new Error("ESP32 rejected command");
        }

        updateRelayDisplay(relay, nextState === "on");

        message.textContent =
            `LED ${relay} is now ${nextState.toUpperCase()}.`;

    } catch (error) {
        console.error("Relay error:", error);

        message.textContent =
            "Unable to control relay. Check ESP32 connection.";

    } finally {
        button.disabled = false;
    }
}

// Make function available to HTML buttons
window.toggleRelay = toggleRelay;

// ==========================================
// AUTOMATIC RELAY REFRESH
// ==========================================

refreshRelayStatus();

setInterval(refreshRelayStatus, 3000);
