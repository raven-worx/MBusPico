from mbus import meterdata, serial
from mbus.devices import *
import config
md = meterdata.MeterData()

dev = kaifa_ma309m_netznoe.Kaifa_MA309M_NetzNoe()
dev.parse_data(data, 'XXXXXXXXXXXXx', md)

