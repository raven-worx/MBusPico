from .microdot_asyncio import Microdot
import sys
import base64

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
	import machine
else:
	import asyncio

METERDATA = None
_AUTH_BASIC = ""

app = Microdot()

async def _reboot():
	await asyncio.sleep(1.5) # [s]
	print("rebooting device...")
	machine.reset()

async def _update():
	await asyncio.sleep(1.5) # [s]
	print("rebooting to USB...")
	machine.bootloader()


@app.before_request
async def authenticate(request):
	if _AUTH_BASIC != "":
		if "Authorization" in request.headers:
			if request.headers["Authorization"] != ("Basic "+_AUTH_BASIC):
				return 'Forbidden', 403
		else:
			return 'Unauthorized', 401, {'WWW-Authenticate': 'Basic realm="xxx"'}

@app.route('/', methods=['GET'])
async def _GET_meterdata(request):
	if METERDATA:
		return METERDATA.to_json(), 200, {'Content-Type': 'application/json'}
	else:
		return None, 204, {'Content-Type': 'application/json'}

@app.route('/reboot', methods=['GET'])
async def _GET_reboot(request):
	if sys.implementation.name == "micropython":
		asyncio.create_task(_reboot())
		return "Device is restarting...", 200
	else:
		return None, 404

@app.route('/update', methods=['GET'])
async def _GET_update(request):
	if sys.implementation.name == "micropython":
		asyncio.create_task(_update())
		return "Device is restarting into USB mode...", 200
	else:
		return None, 404


async def run(config):
	global _AUTH_BASIC
	if config.MBUSPICO_HTTP_AUTH_USER != "" or config.MBUSPICO_HTTP_AUTH_PWD != "":
		_AUTH_BASIC = base64.b64encode((config.MBUSPICO_HTTP_AUTH_USER + ":" + config.MBUSPICO_HTTP_AUTH_PWD).encode('utf-8')).decode('ascii')
		print("AUTH:", _AUTH_BASIC)
	await app.start_server(host='0.0.0.0', port=config.MBUSPICO_HTTP_SERVER_PORT, debug=True, ssl=None)

async def stop():
	await app.shutdown()