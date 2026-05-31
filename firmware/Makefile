compile: clean
	idf.py -B build-release -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" build size

deploy: compile
	idf.py -B build-release -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" flash monitor

# Write device name to NVS without reflashing the full firmware.
# The name is sent as VendorData during Thread commissioning.
#
#   make set-name NAME="Leuchtturm West"
#   make set-name NAME="Leuchtturm West" PORT=/dev/cu.usbmodem1101
set-name:
	@[ -n "$(NAME)" ] || { echo "Usage: make set-name NAME=\"Leuchtturm West\" [PORT=/dev/cu.usbmodemXXXX]"; exit 1; }
	@bash scripts/set_device_name.sh "$(NAME)" $(PORT)

clean:
	rm -rf build
	rm -rf build-release
	rm -rf sdkconfig
	rm -rf dependencies.lock

.PHONY: compile deploy set-name clean
