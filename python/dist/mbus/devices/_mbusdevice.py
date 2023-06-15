import sys
import binascii
if sys.implementation.name == "micropython":
	from cryptolib import aes
else:
	from Cryptodome.Cipher import AES

class _MBusDevice:
	def __init__(self):
		pass
	
	def strftime(self, ts):
		if sys.implementation.name == "micropython":
			return "%04d-%02d-%02dT%02d:%02d:%02dZ" % (ts.year,ts.month,ts.day,ts.hour,ts.minute,ts.second,)
		else:
			return ts.strftime("%Y-%m-%dT%H:%M:%SZ")
	
	#
	# AES-GCM
	#
	def _decrypt_aes_gcm(self, key, data, iv):
		if sys.implementation.name == "micropython":
			# since we trust that we receive only SmartMeter data here
			# we skip data verification and only decrypt using AES-CTR instead of AES-GCM 
			if len(iv) == 12 and len(data) > 0:
				padded_iv = iv + b'\x00\x00\x00\x02' # used as initial counter value in AES-CTR
				MODE_CTR = 6
				cipher = aes(key, MODE_CTR, padded_iv)
				len_data = len(data)
				if 0 != len_data % 16:
					padded_data = data + b'\x00' * (16 - len_data % 16)
				else:
					padded_data = data
				return cipher.decrypt(padded_data)[:len_data]
			else:
				print("Decrypt init error: IV length: 12==",len(iv),", DATA len:", len(data),">0")
				return bytes()
		else:
			cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
			return cipher.decrypt(data)