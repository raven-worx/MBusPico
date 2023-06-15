
class MeterData:
	def __init__(self):
		self.activePowerPlus = 0.0		# [W]
		self.activePowerMinus = 0.0		# [W]
		self.activeEnergyPlus = 0.0		# [Wh]
		self.activeEnergyMinus = 0.0	# [Wh]
		self.reactiveEnergyPlus = 0.0
		self.reactiveEnergyMinus = 0.0
		self.voltageL1 = 0.0			# [V]
		self.voltageL2 = 0.0			# [V]
		self.voltageL3 = 0.0			# [V]
		self.currentL1 = 0.0			# [A]
		self.currentL2 = 0.0			# [A]
		self.currentL3 = 0.0			# [A]
		self.powerFactor = 0.0
		self.timestamp = ""				# 0000-00-00T00:00:00Z
		self.lxTimestamp = 0			# Loxone timestamp (seconds since 1.1.2009)
		self.meterNumber = ""			# 123456789012
	
	def to_json(self):
		j = "{\n"
		j += "\"timestamp\": \"" + self.timestamp + "\",\n"
		j += "\"lxTimestamp\": " + str(self.lxTimestamp) + ",\n"
		j += "\"meterNumber\": \"" + self.meterNumber + "\",\n"
		j += "\"activePowerPlus\": " + ("%.2f" % self.activePowerPlus) + ",\n"
		j += "\"activePowerMinus\": " + ("%.2f" % self.activePowerMinus) + ",\n"
		j += "\"activeEnergyPlus\": " + ("%.2f" % self.activeEnergyPlus) + ",\n"
		j += "\"activeEnergyMinus\": " + ("%.2f" % self.activeEnergyMinus) + ",\n"
		j += "\"voltageL1\": " + ("%.2f" % self.voltageL1) + ",\n"
		j += "\"voltageL2\": " + ("%.2f" % self.voltageL2) + ",\n"
		j += "\"voltageL3\": " + ("%.2f" % self.voltageL3) + ",\n"
		j += "\"currentL1\": " + ("%.2f" % self.currentL1) + ",\n"
		j += "\"currentL2\": " + ("%.2f" % self.currentL2) + ",\n"
		j += "\"currentL3\": " + ("%.2f" % self.currentL3) + ",\n"
		j += "\"powerFactor\": " + ("%.2f" % self.powerFactor) + "\n"
		j += "}"
		return j
