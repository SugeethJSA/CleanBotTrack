import './style.css'

const connectScreen = document.getElementById('connect-screen')
const dashboardScreen = document.getElementById('dashboard-screen')
const ipInput = document.getElementById('ip-address')
const btnConnect = document.getElementById('btn-connect')
const btnDisconnect = document.getElementById('btn-disconnect')
const connStatus = document.getElementById('conn-status')

// Telemetry Elements
const elState = document.getElementById('val-state')
const elDist = document.getElementById('val-dist')
const elIrL = document.getElementById('val-irl')
const elIrR = document.getElementById('val-irr')

let ws = null

function connectWebSocket(ip) {
  connStatus.innerText = 'Connecting...'
  
  // The ESP32 WebSocketsServer uses port 81 by default.
  ws = new WebSocket(`ws://${ip}:81/`)

  ws.onopen = () => {
    connStatus.innerText = 'Connected!'
    setTimeout(() => {
      connectScreen.classList.add('hidden')
      dashboardScreen.classList.remove('hidden')
    }, 500)
  }

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data)
      updateTelemetry(data)
    } catch (e) {
      console.warn("Non-JSON message received:", event.data)
    }
  }

  ws.onclose = () => {
    disconnect()
    connStatus.innerText = 'Disconnected. Connection lost.'
  }

  ws.onerror = (error) => {
    connStatus.innerText = 'Error connecting to ' + ip
    console.error('WebSocket Error', error)
    ws.close()
  }
}

function disconnect() {
  if (ws) {
    ws.close()
    ws = null
  }
  dashboardScreen.classList.add('hidden')
  connectScreen.classList.remove('hidden')
}

function updateTelemetry(data) {
  elState.innerText = data.mode
  elDist.innerText = data.dist

  if (data.irL === 0) {
    elIrL.innerHTML = '<span class="val-alert">OBSTACLE</span>'
  } else {
    elIrL.innerText = 'CLEAR'
  }

  if (data.irR === 0) {
    elIrR.innerHTML = '<span class="val-alert">OBSTACLE</span>'
  } else {
    elIrR.innerText = 'CLEAR'
  }
}

function sendCommand(cmd) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(cmd)
  }
}

// Event Listeners
btnConnect.addEventListener('click', () => {
  const ip = ipInput.value.trim()
  if (ip) {
    localStorage.setItem('cleanbot_ip', ip)
    connectWebSocket(ip)
  }
})

btnDisconnect.addEventListener('click', disconnect)

// D-Pad Controls
const dbtns = document.querySelectorAll('.dbtn')
dbtns.forEach(btn => {
  const cmd = btn.getAttribute('data-cmd')
  
  // Mouse / Touch down -> Send command
  const startAction = (e) => {
    e.preventDefault()
    sendCommand(cmd)
  }
  
  // Mouse / Touch up -> Stop command (unless it's the stop button itself)
  const stopAction = (e) => {
    e.preventDefault()
    if (cmd !== 'S') sendCommand('S')
  }

  btn.addEventListener('mousedown', startAction)
  btn.addEventListener('touchstart', startAction)
  
  btn.addEventListener('mouseup', stopAction)
  btn.addEventListener('touchend', stopAction)
  btn.addEventListener('mouseleave', stopAction) // Failsafe
})

// Speed Control Sliders
const speedSliderL = document.getElementById('speed-slider-l')
const speedValL = document.getElementById('speed-val-l')

const speedSliderR = document.getElementById('speed-slider-r')
const speedValR = document.getElementById('speed-val-r')

speedSliderL.addEventListener('input', (e) => {
  speedValL.innerText = e.target.value
})
speedSliderL.addEventListener('change', (e) => {
  sendCommand(`SPDL:${e.target.value}`)
  localStorage.setItem('cleanbot_spdl', e.target.value)
})

speedSliderR.addEventListener('input', (e) => {
  speedValR.innerText = e.target.value
})
speedSliderR.addEventListener('change', (e) => {
  sendCommand(`SPDR:${e.target.value}`)
  localStorage.setItem('cleanbot_spdr', e.target.value)
})

// Action Buttons
document.getElementById('btn-auto').addEventListener('click', () => sendCommand('AUTO'))
document.getElementById('btn-vac-on').addEventListener('click', () => sendCommand('V1'))
document.getElementById('btn-vac-off').addEventListener('click', () => sendCommand('V0'))

// Keepalive / Dead-man switch heartbeat
setInterval(() => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send('PING')
  }
}, 500)

// Load settings from local storage on startup
window.addEventListener('DOMContentLoaded', () => {
  const savedIp = localStorage.getItem('cleanbot_ip')
  if (savedIp) {
    ipInput.value = savedIp
  }

  const savedSpdL = localStorage.getItem('cleanbot_spdl')
  if (savedSpdL) {
    speedSliderL.value = savedSpdL
    speedValL.innerText = savedSpdL
  }

  const savedSpdR = localStorage.getItem('cleanbot_spdr')
  if (savedSpdR) {
    speedSliderR.value = savedSpdR
    speedValR.innerText = savedSpdR
  }
})


