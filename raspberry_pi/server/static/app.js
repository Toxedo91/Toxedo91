const tableBody = document.getElementById("motor-table-body");
const onlineCount = document.getElementById("online-count");
const offlineCount = document.getElementById("offline-count");
const warningCount = document.getElementById("warning-count");
const maxCount = document.getElementById("max-count");

function createStatusCell(motor) {
  const fragment = document.createDocumentFragment();
  const dot = document.createElement("span");

  dot.classList.add("status-dot");

  if (motor.warning) {
    dot.classList.add("dot-warning");
    fragment.append(dot, document.createTextNode("Warnung"));
    return fragment;
  }

  if (motor.is_online) {
    dot.classList.add("dot-online");
    fragment.append(dot, document.createTextNode("Online"));
    return fragment;
  }

  dot.classList.add("dot-offline");
  fragment.append(dot, document.createTextNode("Offline"));
  return fragment;
}

function buildRow(motor) {
  const row = document.createElement("tr");
  row.className = motor.warning ? "warning" : motor.is_online ? "online" : "offline";

  const columns = [
    motor.motor_id,
    null,
    motor.position_mm.toFixed(2),
    motor.speed_mm_s.toFixed(2),
    motor.ram_usage_percent.toFixed(1),
    motor.cpu_temp_c.toFixed(1),
    motor.error ? motor.error_text : "-",
    motor.age_s,
  ];

  columns.forEach((value, index) => {
    const cell = document.createElement("td");

    if (index === 1) {
      cell.appendChild(createStatusCell(motor));
    } else {
      cell.textContent = value;
    }

    row.appendChild(cell);
  });

  return row;
}

function render(data) {
  const { summary, motors } = data;

  onlineCount.textContent = summary.online_count;
  offlineCount.textContent = summary.offline_count;
  warningCount.textContent = summary.warning_count;
  maxCount.textContent = summary.max_motors;

  const visible = motors.filter((m) => !m.hidden);

  tableBody.replaceChildren(...visible.map(buildRow));
}

async function initialLoad() {
  const resp = await fetch("/api/status");
  const data = await resp.json();
  render(data);
}

const socket = io();
socket.on("dashboard_update", render);

initialLoad().catch((err) => {
  console.error("Initial load failed", err);
});
