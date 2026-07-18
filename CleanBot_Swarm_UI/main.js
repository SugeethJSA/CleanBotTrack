import './style.css'

const connectScreen = document.getElementById('connect-screen')
const dashboardScreen = document.getElementById('dashboard-screen')
const ipInputA = document.getElementById('ip-address-a')
const ipInputB = document.getElementById('ip-address-b')
const btnConnect = document.getElementById('btn-connect')
const btnDisconnect = document.getElementById('btn-disconnect')
const connStatus = document.getElementById('conn-status')

let wsA = null
let wsB = null

function connectSwarm(ipA, ipB) {
  connStatus.innerText = 'Connecting to swarm...'
  
  if (ipA) {
    wsA = new WebSocket(`ws://${ipA}:81/`)
    wsA.onopen = () => checkSwarmConnection()
    wsA.onmessage = (e) => updateTelemetry('a', e.data)
    wsA.onclose = () => { wsA = null; checkSwarmConnection() }
  }
  
  if (ipB) {
    wsB = new WebSocket(`ws://${ipB}:81/`)
    wsB.onopen = () => checkSwarmConnection()
    wsB.onmessage = (e) => updateTelemetry('b', e.data)
    wsB.onclose = () => { wsB = null; checkSwarmConnection() }
  }
}

function checkSwarmConnection() {
  if (wsA && wsA.readyState === WebSocket.OPEN && wsB && wsB.readyState === WebSocket.OPEN) {
    connStatus.innerText = 'Swarm Fully Connected!'
    setTimeout(() => {
      connectScreen.classList.add('hidden')
      dashboardScreen.classList.remove('hidden')
    }, 500)
  } else if ((wsA && wsA.readyState === WebSocket.OPEN) || (wsB && wsB.readyState === WebSocket.OPEN)) {
    connStatus.innerText = 'Partial Swarm Connection...'
  } else {
    connStatus.innerText = 'Swarm Disconnected.'
    dashboardScreen.classList.add('hidden')
    connectScreen.classList.remove('hidden')
  }
}

function disconnectSwarm() {
  if (wsA) wsA.close()
  if (wsB) wsB.close()
}

function updateTelemetry(robot, jsonString) {
  try {
    const data = JSON.parse(jsonString)
    document.getElementById(`val-state-${robot}`).innerText = data.mode
    document.getElementById(`val-dist-${robot}`).innerText = data.dist
    document.getElementById(`val-irl-${robot}`).innerHTML = data.irL === 0 ? '<span class="val-alert">OBSTACLE</span>' : 'CLEAR'
    document.getElementById(`val-irr-${robot}`).innerHTML = data.irR === 0 ? '<span class="val-alert">OBSTACLE</span>' : 'CLEAR'
  } catch (e) {}
}

function sendToA(cmd) { if (wsA && wsA.readyState === WebSocket.OPEN) wsA.send(cmd) }
function sendToB(cmd) { if (wsB && wsB.readyState === WebSocket.OPEN) wsB.send(cmd) }
function sendToSwarm(cmd) { sendToA(cmd); sendToB(cmd); }

// Event Listeners
btnConnect.addEventListener('click', () => {
  const ipA = ipInputA.value.trim()
  const ipB = ipInputB.value.trim()
  if (ipA || ipB) {
    localStorage.setItem('cleanbot_ipa', ipA)
    localStorage.setItem('cleanbot_ipb', ipB)
    connectSwarm(ipA, ipB)
  }
})

btnDisconnect.addEventListener('click', disconnectSwarm)

document.getElementById('btn-swarm-auto').addEventListener('click', () => sendToSwarm('AUTO'))

document.getElementById('btn-auto-a').addEventListener('click', () => sendToA('AUTO'))
document.getElementById('btn-stop-a').addEventListener('click', () => sendToA('S'))

document.getElementById('btn-auto-b').addEventListener('click', () => sendToB('AUTO'))
document.getElementById('btn-stop-b').addEventListener('click', () => sendToB('S'))

setInterval(() => {
  sendToSwarm('PING')
}, 500)

window.addEventListener('DOMContentLoaded', () => {
  const savedIpA = localStorage.getItem('cleanbot_ipa')
  const savedIpB = localStorage.getItem('cleanbot_ipb')
  if (savedIpA) ipInputA.value = savedIpA
  if (savedIpB) ipInputB.value = savedIpB
})


