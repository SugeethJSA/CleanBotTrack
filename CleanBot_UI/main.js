import './style.css'

// Fleet State
let robots = []
let selectedRobotId = null

// DOM Elements
const fleetList = document.getElementById('fleet-list')
const btnAddBot = document.getElementById('btn-add-bot')
const inputBotName = document.getElementById('new-bot-name')
const inputBotIp = document.getElementById('new-bot-ip')
const activeCountBadge = document.getElementById('active-count')

// Active Robot Workspace UI Elements
const activeBotTitle = document.getElementById('active-bot-title')
const activeBotIpDisplay = document.getElementById('active-bot-ip-display')
const activeBotBadge = document.getElementById('active-bot-badge')
const elState = document.getElementById('val-state')
const elDist = document.getElementById('val-dist')
const distBar = document.getElementById('dist-bar')
const elIrL = document.getElementById('val-irl')
const elIrR = document.getElementById('val-irr')
const speedSliderL = document.getElementById('speed-slider-l')
const speedValL = document.getElementById('speed-val-l')
const speedSliderR = document.getElementById('speed-slider-r')
const speedValR = document.getElementById('speed-val-r')
const btnAuto = document.getElementById('btn-auto')
const btnVacOn = document.getElementById('btn-vac-on')
const btnVacOff = document.getElementById('btn-vac-off')
const fanIcon = document.getElementById('fan-icon')

// D-Pad Driving Buttons
const btnDriveF = document.getElementById('btn-drive-f')
const btnDriveL = document.getElementById('btn-drive-l')
const btnDriveS = document.getElementById('btn-drive-s')
const btnDriveR = document.getElementById('btn-drive-r')
const btnDriveB = document.getElementById('btn-drive-b')

// Broadcast & Logging
const btnAllAuto = document.getElementById('btn-all-auto')
const btnAllStop = document.getElementById('btn-all-stop')
const btnAllVacOn = document.getElementById('btn-all-vacon')
const btnAllVacOff = document.getElementById('btn-all-vacoff')
const consoleLog = document.getElementById('console-log')
const btnClearConsole = document.getElementById('btn-clear-console')
const btnResetMap = document.getElementById('btn-reset-map')

// Fleet Colors (HSL for beautiful distinct glows)
const FLEET_COLORS = [
  'hsl(190, 100%, 50%)', // Neon Blue
  'hsl(290, 100%, 60%)', // Neon Magenta
  'hsl(120, 100%, 50%)', // Neon Green
  'hsl(45, 100%, 50%)',  // Neon Yellow
  'hsl(14, 100%, 50%)',  // Neon Orange
  'hsl(0, 100%, 55%)'    // Neon Red
]

// 2D Map Canvas Context
const canvas = document.getElementById('radar-canvas')
const ctx = canvas.getContext('2d')

// Keyboard Drive Control Tracking
let activeKeys = {}
let lastSentDriveCmd = null

// Initialize Canvas Size
function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect()
  canvas.width = rect.width
  canvas.height = Math.max(300, rect.width * 0.65)
}
window.addEventListener('resize', resizeCanvas)
resizeCanvas()

// ---------------- LOCAL STORAGE & FLEET INITIALIZATION ----------------
function loadFleet() {
  const savedFleet = localStorage.getItem('cleanbot_fleet')
  if (savedFleet) {
    try {
      const parsed = JSON.parse(savedFleet)
      robots = parsed.map((bot, idx) => createRobotObject(bot.ip, bot.name, idx))
    } catch (e) {
      console.error("Failed to parse saved fleet", e)
      loadDefaultFleet()
    }
  } else {
    loadDefaultFleet()
  }
}

function loadDefaultFleet() {
  robots = [
    createRobotObject('192.168.1.50', 'Living Room Bot', 0),
    createRobotObject('192.168.4.1', 'AP Mode Bot', 1)
  ]
  saveFleet()
}

function saveFleet() {
  const fleetData = robots.map(bot => ({ ip: bot.ip, name: bot.name }))
  localStorage.setItem('cleanbot_fleet', JSON.stringify(fleetData))
}

function createRobotObject(ip, name, index) {
  return {
    id: ip,
    name: name || `CleanBot-${index + 1}`,
    ip: ip,
    ws: null,
    status: 'disconnected', // 'disconnected', 'connecting', 'connected'
    pingInterval: null,
    latency: null,
    pingTime: null,
    telemetry: {
      mode: 'S',
      auto: 0,
      dist: 999,
      irL: 1,
      irR: 1,
      spdL: 200,
      spdR: 200,
      vac: 0
    },
    // Map tracking
    color: FLEET_COLORS[index % FLEET_COLORS.length],
    pos: { x: canvas.width / 2 + (index - 1) * 40, y: canvas.height / 2 },
    heading: -Math.PI / 2, // facing up
    path: []
  }
}

// ---------------- LOGGING UTILITY ----------------
function logMessage(botName, text, type = 'system') {
  const time = new Date().toLocaleTimeString()
  const div = document.createElement('div')
  div.className = `console-line ${type}`
  
  if (botName) {
    div.innerHTML = `<span class="timestamp">[${time}]</span> <span class="bot-tag">[${botName}]</span> ${text}`
  } else {
    div.innerHTML = `<span class="timestamp">[${time}]</span> ${text}`
  }
  
  consoleLog.appendChild(div)
  consoleLog.scrollTop = consoleLog.scrollHeight
}

// ---------------- WEBSOCKET CONNECTION MANAGEMENT ----------------
function connectRobot(robot) {
  if (robot.ws) {
    robot.ws.close()
  }

  robot.status = 'connecting'
  updateFleetUI()
  logMessage(robot.name, `Initiating WebSocket connection to ws://${robot.ip}:81/...`, 'system')

  try {
    const ws = new WebSocket(`ws://${robot.ip}:81/`)
    robot.ws = ws

    ws.onopen = () => {
      robot.status = 'connected'
      logMessage(robot.name, 'Connected successfully!', 'success')
      updateFleetUI()
      updateHeaderCount()

      // Start dead-man ping heartbeat
      robot.pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          robot.pingTime = performance.now()
          ws.send('PING')
        }
      }, 800)
    }

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data)
        
        // Check if this is a reply to our ping
        if (robot.pingTime) {
          robot.latency = Math.round(performance.now() - robot.pingTime)
          robot.pingTime = null
        }

        // Update telemetry data
        robot.telemetry = { ...robot.telemetry, ...data }

        // If this is the active robot, render its data to the console/UI
        if (selectedRobotId === robot.id) {
          updateSelectedRobotUI(robot)
        }
        
        // Live alert feedback
        if (robot.telemetry.mode === 'S' && data.msg && data.msg.includes('ALERT')) {
          logMessage(robot.name, `ALERT: ${data.msg}`, 'alert')
        }
      } catch (e) {
        // Non-JSON message from ESP32
        logMessage(robot.name, `Log: ${event.data}`, 'bot-msg')
      }
    }

    ws.onclose = () => {
      handleDisconnect(robot)
    }

    ws.onerror = (err) => {
      logMessage(robot.name, `Connection error. Verify IP address.`, 'alert')
      handleDisconnect(robot)
    }
  } catch (e) {
    logMessage(robot.name, `Connection failed: ${e.message}`, 'alert')
    handleDisconnect(robot)
  }
}

function handleDisconnect(robot) {
  if (robot.pingInterval) {
    clearInterval(robot.pingInterval)
    robot.pingInterval = null
  }
  robot.ws = null
  robot.latency = null
  if (robot.status !== 'disconnected') {
    robot.status = 'disconnected'
    logMessage(robot.name, 'Disconnected.', 'info')
    updateFleetUI()
    updateHeaderCount()
    if (selectedRobotId === robot.id) {
      updateSelectedRobotUI(robot)
    }
  }
}

function sendRobotCommand(robot, cmd) {
  if (robot && robot.ws && robot.ws.readyState === WebSocket.OPEN) {
    robot.ws.send(cmd)
  }
}

// ---------------- FLEET & SELECTION UI RENDERING ----------------
function updateHeaderCount() {
  const connectedCount = robots.filter(r => r.status === 'connected').length
  activeCountBadge.innerText = `${connectedCount} / ${robots.length}`
  if (connectedCount > 0) {
    activeCountBadge.className = 'count-badge pulse-green'
  } else {
    activeCountBadge.className = 'count-badge'
  }
}

function updateFleetUI() {
  fleetList.innerHTML = ''
  
  if (robots.length === 0) {
    fleetList.innerHTML = '<div class="no-bots-msg">No robots registered. Add one above.</div>'
    return
  }

  robots.forEach(robot => {
    const card = document.createElement('div')
    const isSelected = robot.id === selectedRobotId
    card.className = `robot-list-card ${robot.status} ${isSelected ? 'selected' : ''}`
    
    // Status text & indicator class
    let statusClass = 'dot-offline'
    let statusText = 'OFFLINE'
    let actionBtnText = 'Connect'
    
    if (robot.status === 'connected') {
      statusClass = 'dot-online'
      statusText = robot.latency ? `${robot.latency}ms` : 'ONLINE'
      actionBtnText = 'Disconnect'
    } else if (robot.status === 'connecting') {
      statusClass = 'dot-connecting'
      statusText = 'CONNECTING'
      actionBtnText = 'Cancel'
    }

    card.innerHTML = `
      <div class="bot-info-row" style="border-left: 4px solid ${robot.color}; padding-left: 8px;">
        <span class="bot-name">${robot.name}</span>
        <span class="bot-ip">${robot.ip}</span>
      </div>
      <div class="bot-status-row">
        <span class="status-indicator-tag"><span class="status-dot ${statusClass}"></span> ${statusText}</span>
        <div class="bot-card-actions">
          <button class="btn-card-toggle btn-mini-action">${actionBtnText}</button>
          <button class="btn-card-delete btn-mini-danger">✖</button>
        </div>
      </div>
    `

    // Click on the card body selects it
    card.addEventListener('click', (e) => {
      if (e.target.tagName !== 'BUTTON') {
        selectRobot(robot.id)
      }
    })

    // Action button listeners
    const toggleBtn = card.querySelector('.btn-card-toggle')
    toggleBtn.addEventListener('click', (e) => {
      e.stopPropagation()
      if (robot.status === 'disconnected') {
        connectRobot(robot)
      } else {
        handleDisconnect(robot)
      }
    })

    const deleteBtn = card.querySelector('.btn-card-delete')
    deleteBtn.addEventListener('click', (e) => {
      e.stopPropagation()
      removeRobot(robot.id)
    })

    fleetList.appendChild(card)
  })
}

function selectRobot(robotId) {
  selectedRobotId = robotId
  updateFleetUI()
  
  const robot = robots.find(r => r.id === robotId)
  if (robot) {
    logMessage(null, `Selected robot: <strong>${robot.name}</strong>`, 'system')
    updateSelectedRobotUI(robot)
  }
}

function updateSelectedRobotUI(robot) {
  // Title & Headers
  activeBotTitle.innerText = robot.name.toUpperCase()
  activeBotIpDisplay.innerText = robot.ip
  
  // Status Badge styling
  activeBotBadge.className = `badge-${robot.status}`
  activeBotBadge.innerText = robot.status.toUpperCase()

  // Controls state: Enable controls only if robot is connected
  const isConnected = robot.status === 'connected'
  
  // Update disabled state for control inputs
  speedSliderL.disabled = !isConnected
  speedSliderR.disabled = !isConnected
  btnAuto.disabled = !isConnected
  btnVacOn.disabled = !isConnected
  btnVacOff.disabled = !isConnected
  
  // D-Pad Enable/Disable
  btnDriveF.disabled = !isConnected
  btnDriveB.disabled = !isConnected
  btnDriveL.disabled = !isConnected
  btnDriveR.disabled = !isConnected
  btnDriveS.disabled = !isConnected

  // Populate actual inputs and fields if connected
  if (isConnected) {
    const t = robot.telemetry
    
    // 1. Telemetry State
    let modeText = 'STOPPED'
    if (t.auto === 1) {
      modeText = `AUTO - `
      if (t.mode === 'F') modeText += 'FORWARD'
      else if (t.mode === 'B') modeText += 'AVOIDING (REVERSE)'
      else if (t.mode === 'L') modeText += 'AVOIDING (LEFT SPIN)'
      else if (t.mode === 'R') modeText += 'AVOIDING (RIGHT SPIN)'
      else modeText += 'STOPPED'
    } else {
      if (t.mode === 'F') modeText = 'MANUAL - FORWARD'
      else if (t.mode === 'B') modeText = 'MANUAL - BACKWARD'
      else if (t.mode === 'L') modeText = 'MANUAL - LEFT'
      else if (t.mode === 'R') modeText = 'MANUAL - RIGHT'
    }
    
    elState.innerText = modeText
    elState.className = t.auto === 1 ? 'value glow-text-green' : 'value'

    // 2. Ultrasonic Distance
    if (t.dist === 999) {
      elDist.innerText = 'CLEAR'
      distBar.style.width = '100%'
      distBar.className = 'progress-bar bg-success'
    } else {
      elDist.innerText = t.dist
      
      // Visual feedback bar: 15cm is close (red), 30cm is warning (orange), 50cm is green
      const percentage = Math.min(100, (t.dist / 60) * 100)
      distBar.style.width = `${percentage}%`
      
      if (t.dist < 15) {
        distBar.className = 'progress-bar bg-danger pulse-red'
        elDist.innerHTML = `<span class="val-alert">${t.dist}</span>`
      } else if (t.dist < 30) {
        distBar.className = 'progress-bar bg-warning'
      } else {
        distBar.className = 'progress-bar bg-success'
      }
    }

    // 3. Front IR States
    if (t.irL === 0) {
      elIrL.innerHTML = '<span class="val-alert">OBSTACLE</span>'
    } else {
      elIrL.innerText = 'CLEAR'
    }

    if (t.irR === 0) {
      elIrR.innerHTML = '<span class="val-alert">OBSTACLE</span>'
    } else {
      elIrR.innerText = 'CLEAR'
    }

    // 4. Speed Sliders values (only change if user isn't currently dragging them)
    if (!speedSliderL.matches(':active')) {
      speedSliderL.value = t.spdL
      speedValL.innerText = t.spdL
    }
    if (!speedSliderR.matches(':active')) {
      speedSliderR.value = t.spdR
      speedValR.innerText = t.spdR
    }

    // 5. Vacuum fan spinner class
    if (t.vac === 1) {
      fanIcon.classList.add('spin-fast')
      btnVacOn.classList.add('active')
      btnVacOff.classList.remove('active')
    } else {
      fanIcon.classList.remove('spin-fast')
      btnVacOn.classList.remove('active')
      btnVacOff.classList.add('active')
    }

    // 6. Highlight active D-pad direction based on motor command
    clearDpadHighlights()
    if (t.mode === 'F') btnDriveF.classList.add('active')
    if (t.mode === 'B') btnDriveB.classList.add('active')
    if (t.mode === 'L') btnDriveL.classList.add('active')
    if (t.mode === 'R') btnDriveR.classList.add('active')
    if (t.mode === 'S') btnDriveS.classList.add('active')

    // AI Button state
    if (t.auto === 1) {
      btnAuto.classList.add('active-auto')
    } else {
      btnAuto.classList.remove('active-auto')
    }
  } else {
    // Clear display values if offline
    elState.innerText = 'N/A'
    elDist.innerText = '--'
    distBar.style.width = '0%'
    elIrL.innerText = 'N/A'
    elIrR.innerText = 'N/A'
    speedValL.innerText = '--'
    speedValR.innerText = '--'
    fanIcon.classList.remove('spin-fast')
    clearDpadHighlights()
    btnAuto.classList.remove('active-auto')
  }
}

function clearDpadHighlights() {
  btnDriveF.classList.remove('active')
  btnDriveB.classList.remove('active')
  btnDriveL.classList.remove('active')
  btnDriveR.classList.remove('active')
  btnDriveS.classList.remove('active')
}

// ---------------- FLEET MANAGEMENT LOGIC ----------------
function addRobot() {
  const name = inputBotName.value.trim()
  const ip = inputBotIp.value.trim()

  if (!ip) {
    alert("Please enter a valid IP Address!")
    return
  }

  // Regex check for basic IP structure
  const ipPattern = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/
  if (!ipPattern.test(ip)) {
    alert("Invalid IP Address format. Example: 192.168.1.50")
    return
  }

  // Check duplicates
  if (robots.find(r => r.ip === ip)) {
    alert("This IP address is already registered in the fleet.")
    return
  }

  const newBot = createRobotObject(ip, name, robots.length)
  robots.push(newBot)
  saveFleet()
  updateFleetUI()
  updateHeaderCount()
  
  // Select the newly added robot
  selectRobot(newBot.id)
  
  // Reset form inputs
  inputBotName.value = ''
  inputBotIp.value = ''

  logMessage(newBot.name, `Registered to fleet C2! Click Connect to link.`, 'success')
}

function removeRobot(id) {
  const index = robots.findIndex(r => r.id === id)
  if (index !== -1) {
    const bot = robots[index]
    logMessage(null, `Removed robot <strong>${bot.name}</strong> (${bot.ip}) from fleet.`, 'info')
    
    // Close ws connection first
    handleDisconnect(bot)
    
    robots.splice(index, 1)
    
    // Regenerate colors/indices for consistency
    robots.forEach((r, idx) => {
      r.color = FLEET_COLORS[idx % FLEET_COLORS.length]
    })

    saveFleet()
    updateFleetUI()
    updateHeaderCount()

    // Reselect another robot if deleted one was selected
    if (selectedRobotId === id) {
      if (robots.length > 0) {
        selectRobot(robots[0].id)
      } else {
        selectedRobotId = null
        activeBotTitle.innerText = "NO BOT SELECTED"
        activeBotIpDisplay.innerText = "---.---.---.---"
        activeBotBadge.className = "badge-offline"
        activeBotBadge.innerText = "OFFLINE"
        updateSelectedRobotUI({ status: 'disconnected', telemetry: {} })
      }
    }
  }
}

// ---------------- RADAR & PATH MAPPING SIMULATOR ----------------
// Dead-reckoning update loop (simulates kinematics since ESP32 does not return absolute position coords)
let lastMapUpdate = performance.now()

function updateRobotPhysics() {
  const now = performance.now()
  const dt = (now - lastMapUpdate) / 1000 // elapsed seconds
  lastMapUpdate = now

  robots.forEach(robot => {
    // We only update motion if the robot is connected and not stationary
    if (robot.status !== 'connected') return

    const t = robot.telemetry
    const speedScale = 0.15 // Scale motor power to coordinate units
    const baseSpd = (t.spdL + t.spdR) / 2
    const linearSpeed = baseSpd * speedScale * dt
    const turnRate = 2.4 * dt // radians per second at full pivot

    if (t.mode === 'F') {
      // Move Forward along heading vector
      robot.pos.x += Math.cos(robot.heading) * linearSpeed
      robot.pos.y += Math.sin(robot.heading) * linearSpeed
    } else if (t.mode === 'B') {
      // Move Backward against heading vector
      robot.pos.x -= Math.cos(robot.heading) * linearSpeed
      robot.pos.y -= Math.sin(robot.heading) * linearSpeed
    } else if (t.mode === 'L') {
      // Spin Left (Counter Clockwise)
      robot.heading -= turnRate
    } else if (t.mode === 'R') {
      // Spin Right (Clockwise)
      robot.heading += turnRate
    }

    // Keep within boundary limits
    const margin = 15
    robot.pos.x = Math.max(margin, Math.min(canvas.width - margin, robot.pos.x))
    robot.pos.y = Math.max(margin, Math.min(canvas.height - margin, robot.pos.y))

    // Record position history for rendering lines (only push if moved significantly)
    if (t.mode !== 'S') {
      const lastPoint = robot.path[robot.path.length - 1]
      if (!lastPoint || Math.hypot(robot.pos.x - lastPoint.x, robot.pos.y - lastPoint.y) > 4) {
        robot.path.push({ x: robot.pos.x, y: robot.pos.y })
        
        // Limit path trail memory
        if (robot.path.length > 250) {
          robot.path.shift()
        }
      }
    }
  })
}

function drawMap() {
  ctx.clearRect(0, 0, canvas.width, canvas.height)

  // 1. Draw Grid Background (Futuristic Tech Radar styling)
  ctx.strokeStyle = 'rgba(56, 189, 248, 0.05)'
  ctx.lineWidth = 1
  
  // Vertical/Horizontal Grid Lines
  const gridSize = 30
  for (let x = 0; x < canvas.width; x += gridSize) {
    ctx.beginPath()
    ctx.moveTo(x, 0)
    ctx.lineTo(x, canvas.height)
    ctx.stroke()
  }
  for (let y = 0; y < canvas.height; y += gridSize) {
    ctx.beginPath()
    ctx.moveTo(0, y)
    ctx.lineTo(canvas.width, y)
    ctx.stroke()
  }

  // Draw concentric radar circles in center
  ctx.strokeStyle = 'rgba(56, 189, 248, 0.07)'
  const centerX = canvas.width / 2
  const centerY = canvas.height / 2
  for (let r = 50; r < Math.max(canvas.width, canvas.height); r += 60) {
    ctx.beginPath()
    ctx.arc(centerX, centerY, r, 0, 2 * Math.PI)
    ctx.stroke()
  }

  // 2. Draw Trails and Robots
  robots.forEach(robot => {
    // Only draw connected or recently connected robots
    if (robot.status !== 'connected' && robot.path.length === 0) return

    // Draw path line
    if (robot.path.length > 1) {
      ctx.beginPath()
      ctx.strokeStyle = robot.color
      ctx.globalAlpha = 0.5
      ctx.lineWidth = 2.5
      ctx.setLineDash([4, 4])
      ctx.moveTo(robot.path[0].x, robot.path[0].y)
      for (let i = 1; i < robot.path.length; i++) {
        ctx.lineTo(robot.path[i].x, robot.path[i].y)
      }
      ctx.stroke()
      ctx.setLineDash([])
      ctx.globalAlpha = 1.0
    }

    const t = robot.telemetry

    // Draw Sensor cones relative to the robot's local coordinates
    ctx.save()
    ctx.translate(robot.pos.x, robot.pos.y)
    ctx.rotate(robot.heading)

    // Render Front Left and Front Right IR field visual cones (180 deg front)
    // IR Left angle is roughly -30 deg, IR Right is +30 deg. Cones extend 25px.
    const drawIRCone = (angleOffset, isObstacle) => {
      ctx.beginPath()
      ctx.moveTo(0, 0)
      ctx.arc(0, 0, 30, angleOffset - 0.25, angleOffset + 0.25)
      ctx.closePath()
      ctx.fillStyle = isObstacle === 0 ? 'rgba(244, 63, 94, 0.35)' : 'rgba(16, 185, 129, 0.08)'
      ctx.fill()
      ctx.strokeStyle = isObstacle === 0 ? 'rgba(244, 63, 94, 0.65)' : 'rgba(16, 185, 129, 0.15)'
      ctx.lineWidth = 1
      ctx.stroke()
    }
    
    // Front IRs (Front-Left: -0.4 rad, Front-Right: 0.4 rad)
    drawIRCone(-0.4, t.irL)
    drawIRCone(0.4, t.irR)

    // Render Rear Ultrasonic field visual cone (Back angle is Math.PI. Cone extends by distance value)
    const rawDist = t.dist
    const isUltraClose = rawDist < 15
    const drawDist = rawDist === 999 ? 65 : Math.max(10, Math.min(80, rawDist * 1.5))
    
    ctx.beginPath()
    ctx.moveTo(0, 0)
    ctx.arc(0, 0, drawDist, Math.PI - 0.45, Math.PI + 0.45)
    ctx.closePath()
    ctx.fillStyle = isUltraClose ? 'rgba(244, 63, 94, 0.35)' : 'rgba(56, 189, 248, 0.08)'
    ctx.fill()
    ctx.strokeStyle = isUltraClose ? 'rgba(244, 63, 94, 0.7)' : 'rgba(56, 189, 248, 0.2)'
    ctx.lineWidth = 1
    ctx.stroke()

    // Draw Robot body
    ctx.beginPath()
    ctx.arc(0, 0, 12, 0, 2 * Math.PI)
    ctx.fillStyle = robot.color
    ctx.shadowBlur = 12
    ctx.shadowColor = robot.color
    ctx.fill()
    ctx.shadowBlur = 0 // Reset shadow

    // Add outline
    ctx.strokeStyle = '#ffffff'
    ctx.lineWidth = 2
    ctx.stroke()

    // Draw orientation arrow (facing forward = right relative to local rotation)
    ctx.beginPath()
    ctx.moveTo(4, 0)
    ctx.lineTo(-4, -5)
    ctx.lineTo(-4, 5)
    ctx.closePath()
    ctx.fillStyle = '#ffffff'
    ctx.fill()

    ctx.restore()

    // If robot is selected, draw a pulsing halo around it on map
    if (robot.id === selectedRobotId) {
      ctx.beginPath()
      ctx.arc(robot.pos.x, robot.pos.y, 20 + Math.sin(performance.now() / 150) * 4, 0, 2 * Math.PI)
      ctx.strokeStyle = robot.color
      ctx.lineWidth = 1.5
      ctx.stroke()
    }

    // Label nickname above robot
    ctx.font = '500 10px Inter'
    ctx.fillStyle = '#e2e8f0'
    ctx.textAlign = 'center'
    ctx.fillText(robot.name, robot.pos.x, robot.pos.y - 24)
  })
}

// ---------------- CANVAS PATH/TRAIL RESET ----------------
btnResetMap.addEventListener('click', () => {
  robots.forEach(r => r.path = [])
  logMessage(null, "Cleared fleet path trails from canvas map.", "system")
})

// ---------------- BROADCAST / COMMAND ALL CONTROLS ----------------
btnAllAuto.addEventListener('click', () => {
  robots.forEach(bot => sendRobotCommand(bot, 'AUTO'))
  logMessage(null, "Broadcast Command: <strong>ALL AUTO</strong> dispatched.", "broadcast")
})

btnAllStop.addEventListener('click', () => {
  robots.forEach(bot => sendRobotCommand(bot, 'S'))
  logMessage(null, "Broadcast Command: <strong>ALL STOP</strong> dispatched.", "broadcast")
})

btnAllVacOn.addEventListener('click', () => {
  robots.forEach(bot => sendRobotCommand(bot, 'V1'))
  logMessage(null, "Broadcast Command: <strong>ALL VACUUM ON</strong> dispatched.", "broadcast")
})

btnAllVacOff.addEventListener('click', () => {
  robots.forEach(bot => sendRobotCommand(bot, 'V0'))
  logMessage(null, "Broadcast Command: <strong>ALL VACUUM OFF</strong> dispatched.", "broadcast")
})

// ---------------- ACTION BUTTON EVENT LISTENERS ----------------
btnAuto.addEventListener('click', () => {
  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (activeBot) {
    const isCurrentlyAuto = activeBot.telemetry.auto === 1
    const nextCmd = isCurrentlyAuto ? 'MANUAL' : 'AUTO'
    sendRobotCommand(activeBot, nextCmd)
  }
})

btnVacOn.addEventListener('click', () => {
  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (activeBot) sendRobotCommand(activeBot, 'V1')
})

btnVacOff.addEventListener('click', () => {
  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (activeBot) sendRobotCommand(activeBot, 'V0')
})

// ---------------- D-PAD BUTTON EVENTS ----------------
const dbtns = document.querySelectorAll('.dbtn')
dbtns.forEach(btn => {
  const cmd = btn.getAttribute('data-cmd')
  
  const startAction = (e) => {
    e.preventDefault()
    const activeBot = robots.find(r => r.id === selectedRobotId)
    if (activeBot) sendRobotCommand(activeBot, cmd)
  }
  
  const stopAction = (e) => {
    e.preventDefault()
    const activeBot = robots.find(r => r.id === selectedRobotId)
    if (activeBot && cmd !== 'S') sendRobotCommand(activeBot, 'S')
  }

  btn.addEventListener('mousedown', startAction)
  btn.addEventListener('touchstart', startAction)
  btn.addEventListener('mouseup', stopAction)
  btn.addEventListener('touchend', stopAction)
  btn.addEventListener('mouseleave', stopAction) // Failsafe
})

// ---------------- SPEED SLIDERS CONTROL ----------------
speedSliderL.addEventListener('input', (e) => {
  speedValL.innerText = e.target.value
})
speedSliderL.addEventListener('change', (e) => {
  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (activeBot) {
    sendRobotCommand(activeBot, `SPDL:${e.target.value}`)
  }
})

speedSliderR.addEventListener('input', (e) => {
  speedValR.innerText = e.target.value
})
speedSliderR.addEventListener('change', (e) => {
  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (activeBot) {
    sendRobotCommand(activeBot, `SPDR:${e.target.value}`)
  }
})

// ---------------- ADD ROBOT EVENT ----------------
btnAddBot.addEventListener('click', addRobot)

// ---------------- KEYBOARD CONTROLS ----------------
function handleKeyDown(e) {
  // Disable keyboard control if typing in text fields
  if (document.activeElement.tagName === 'INPUT') return

  const key = e.code
  if (activeKeys[key]) return // Ignore repeats
  activeKeys[key] = true

  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (!activeBot || activeBot.status !== 'connected') return

  let cmd = null

  if (key === 'ArrowUp' || key === 'KeyW') cmd = 'F'
  else if (key === 'ArrowDown' || key === 'KeyS') cmd = 'B'
  else if (key === 'ArrowLeft' || key === 'KeyA') cmd = 'L'
  else if (key === 'ArrowRight' || key === 'KeyD') cmd = 'R'
  else if (key === 'Space' || key === 'KeyX') cmd = 'S'
  else if (key === 'KeyV') {
    // Toggle vacuum
    const currentVac = activeBot.telemetry.vac
    cmd = currentVac === 1 ? 'V0' : 'V1'
  } else if (key === 'KeyQ' || key === 'KeyM') {
    // Toggle auto mode
    const currentAuto = activeBot.telemetry.auto
    cmd = currentAuto === 1 ? 'MANUAL' : 'AUTO'
  }

  if (cmd) {
    sendRobotCommand(activeBot, cmd)
    lastSentDriveCmd = cmd
    
    // Visual feedback on Dpad buttons
    highlightDpadBtn(cmd)
  }
}

function handleKeyUp(e) {
  const key = e.code
  delete activeKeys[key]

  const activeBot = robots.find(r => r.id === selectedRobotId)
  if (!activeBot || activeBot.status !== 'connected') return

  // If a direction key is released, stop robot (unless another direction key is still down)
  const isDirKey = ['ArrowUp', 'KeyW', 'ArrowDown', 'KeyS', 'ArrowLeft', 'KeyA', 'ArrowRight', 'KeyD'].includes(key)
  
  if (isDirKey) {
    const stillDriving = ['ArrowUp', 'KeyW', 'ArrowDown', 'KeyS', 'ArrowLeft', 'KeyA', 'ArrowRight', 'KeyD'].some(k => activeKeys[k])
    if (!stillDriving) {
      sendRobotCommand(activeBot, 'S')
      lastSentDriveCmd = null
      clearDpadHighlights()
    }
  }
}

function highlightDpadBtn(cmd) {
  clearDpadHighlights()
  if (cmd === 'F') btnDriveF.classList.add('active')
  if (cmd === 'B') btnDriveB.classList.add('active')
  if (cmd === 'L') btnDriveL.classList.add('active')
  if (cmd === 'R') btnDriveR.classList.add('active')
  if (cmd === 'S') btnDriveS.classList.add('active')
}

window.addEventListener('keydown', handleKeyDown)
window.addEventListener('keyup', handleKeyUp)

// ---------------- SYSTEM CONSOLE CLEAR ----------------
btnClearConsole.addEventListener('click', () => {
  consoleLog.innerHTML = `<div class="console-line system">[SYSTEM] Logs cleared. Console ready.</div>`
})

// ---------------- PHYSICS & ANIMATION RENDER LOOP ----------------
function renderLoop() {
  updateRobotPhysics()
  drawMap()
  requestAnimationFrame(renderLoop)
}

// ---------------- INITIALIZE PAGE ----------------
window.addEventListener('DOMContentLoaded', () => {
  loadFleet()
  updateFleetUI()
  updateHeaderCount()
  
  // Select first bot by default if available
  if (robots.length > 0) {
    selectRobot(robots[0].id)
  }
  
  // Start the background physics simulation and map rendering loop
  requestAnimationFrame(renderLoop)
  
  logMessage(null, "Dashboard fleet system online. Ready to connect.", "success")
})
