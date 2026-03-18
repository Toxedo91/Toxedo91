const tableBody = document.getElementById("motor-table-body");
const onlineCount = document.getElementById("online-count");
const offlineCount = document.getElementById("offline-count");
const warningCount = document.getElementById("warning-count");
const maxCount = document.getElementById("max-count");

function statusBadge(motor) {
  if (motor.warning) {
    return `<span class="status-dot dot-warning"></span>Warnung`;
  }
  if (motor.is_online) {
    return `<span class="status-dot dot-online"></span>Online`;
  }
  return `<span class="status-dot dot-offline"></span>Offline`;
}

function render(data) {
  const { summary, motors } = data;

  onlineCount.textContent = summary.online_count;
  offlineCount.textContent = summary.offline_count;
  warningCount.textContent = summary.warning_count;
  maxCount.textContent = summary.max_motors;

  const visible = motors.filter((m) => !m.hidden);

  tableBody.innerHTML = visible
    .map((m) => {
      const rowClass = m.warning ? "warning" : m.is_online ? "online" : "offline";
      return `
      <tr class="${rowClass}">
        <td>${m.motor_id}</td>
        <td>${statusBadge(m)}</td>
        <td>${m.position_mm.toFixed(2)}</td>
        <td>${m.speed_mm_s.toFixed(2)}</td>
        <td>${m.ram_usage_percent.toFixed(1)}</td>
        <td>${m.cpu_temp_c.toFixed(1)}</td>
        <td>${m.error ? m.error_text : "-"}</td>
        <td>${m.age_s}</td>
      </tr>`;
    })
    .join("");
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
