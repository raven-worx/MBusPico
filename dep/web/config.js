function _RemoveResultAlerts() {
	var alerts = document.querySelectorAll('.alert.result-alert')
	alerts.forEach(function(alert) {
  		alert.remove()
	})
}

function _ShowSuccessMessage(msg) {
	_RemoveResultAlerts()
	document.body.insertAdjacentHTML('beforeend',`
	<div class="alert alert-success mx-auto show fade d-flex align-items-center fixed-bottom result-alert" role="alert">
	<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" fill="currentColor" class="bi bi-exclamation-triangle-fill flex-shrink-0 me-2" viewBox="0 0 16 16" role="img">
	<path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zm-3.97-3.03a.75.75 0 0 0-1.08.022L7.477 9.417 5.384 7.323a.75.75 0 0 0-1.06 1.06L6.97 11.03a.75.75 0 0 0 1.079-.02l3.992-4.99a.75.75 0 0 0-.01-1.05z"/>
	</svg>
	<div class="flex-fill">${msg}</div>
	<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
	</div>
	`)
}

function _ShowErrorMessage(msg) {
	_RemoveResultAlerts()
	document.body.insertAdjacentHTML('beforeend',`
	<div class="alert alert-danger mx-auto show fade d-flex align-items-center fixed-bottom result-alert" role="alert">
	<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" fill="currentColor" class="bi bi-exclamation-triangle-fill flex-shrink-0 me-2" viewBox="0 0 16 16" role="img">
		<path d="M8.982 1.566a1.13 1.13 0 0 0-1.96 0L.165 13.233c-.457.778.091 1.767.98 1.767h13.713c.889 0 1.438-.99.98-1.767L8.982 1.566zM8 5c.535 0 .954.462.9.995l-.35 3.507a.552.552 0 0 1-1.1 0L7.1 5.995A.905.905 0 0 1 8 5zm.002 6a1 1 0 1 1 0 2 1 1 0 0 1 0-2z"/>
	</svg>
	<div class="flex-fill">${msg}</div>
	<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
	</div>
	`)
}

function SaveSettings() {
	var btn_save = document.querySelector('button#btn_save')
	btn_save.disabled = true
	btn_save.insertAdjacentHTML('afterbegin','<span class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span> ')
	var formData = new FormData(document.getElementById('config_form'));
	var req = new XMLHttpRequest();
	req.withCredentials = true;
	req.onreadystatechange = function() {
		if(this.readyState == 4) {
			document.querySelector('button#btn_save span').remove()
			btn_save.disabled = false
			_RemoveResultAlerts()
			switch (this.status) {
				case 200:
				case 202:
					_ShowSuccessMessage('Successfully saved configuration. The device is rebooting now...')
					break;
				default:
					_ShowErrorMessage('Failed to save configuration.')
					break;
			}
		}
	}
	req.open('POST', '/config/data', true);
	req.send(formData);
}

function LoadSettings() {
	var req = new XMLHttpRequest();
	req.withCredentials = true;
	req.onreadystatechange = function() {
		if(this.readyState == 4) {
			if (this.status == 200)
			{
			var j = JSON.parse(req.responseText)
			document.getElementById("version_text").innerHTML  = "v" + j.version
			document.getElementById("meter_key").value = j.meter.key
			document.getElementById("wifi_ssid").value = j.wifi.ssid
			document.getElementById("wifi_hostname").value = j.wifi.hostname
			document.getElementById("http_port").value = j.http.port
			document.getElementById("http_auth_user").value = j.http.user
			document.getElementById("http_auth_pwd").value = j.http.pwd
			document.getElementById("udp_enabled").checked = j.udp.enabled
			document.getElementById("udp_receiver").value = j.udp.receiver
			document.getElementById("udp_port").value = j.udp.port
			document.getElementById("udp_interval").value = j.udp.interval
			}
		}
	}
	req.open('GET', '/config/data', true);
	req.send();
}

function Reboot(update) {
	var req = new XMLHttpRequest();
	req.withCredentials = true;
	req.onreadystatechange = function() {
		if(this.readyState == 4) {
			_RemoveResultAlerts()
			switch (this.status) {
				case 200:
				case 202:
					_ShowSuccessMessage(update ? 'Device is rebooting into USB update mode now...' : 'Device is rebooting now...')
					break;
				default:
					_ShowErrorMessage('Failed to initiate reboot of device.')
					break;
			}
		}
	}
	req.open('GET', update ? '/update' : '/reboot', true);
	req.send();
}

function _AppendLog(msg) {
	var log = document.getElementById('debug_log')
	var atbottom = log.scrollTop == log.scrollHeight
	log.value += (msg + '\n')
	if (atbottom) {
		log.scrollTop = log.scrollHeight
	}
}

var _WS = null;
function SetupWebsocket() {
	_WS = new WebSocket('ws://' + window.location.host + '/stream', 'MBusPico')
	_WS.onopen = (event) => {
		//console.log("WS opened: ", event);
	}
	_WS.onerror = (event) => {
		console.error("WS error: ", event);
	}
	_WS.onclose = (event) => {
		// TODO: auto-reconnect?
		console.log("WS closed")
		// event.code
		// event.reason
		// event.wasClean
	}
	_WS.onmessage = (event) => {
		const d = JSON.parse(event.data)
		switch(d['type']) {
			case 'log':
				msg = decodeURIComponent(d['data'])
				_AppendLog(msg)
				break;
		}
	}
}

var _DOMReady = function(a,b,c){b=document,c='addEventListener';b[c]?b[c]('DOMContentLoaded',a):window.attachEvent('onload',a)}
_DOMReady(function() {
	LoadSettings()
	SetupWebsocket()
})
