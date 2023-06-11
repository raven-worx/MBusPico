from Cryptodome.Cipher import AES

class _MBusDevice:
	def __init__(self):
		pass
	
	#
	# AES-GCM
	#
	def _decrypt_aes_gcm(self, key, data, iv):
		cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
		return cipher.decrypt(data)