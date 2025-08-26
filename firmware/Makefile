compile: clean
	idf.py -B build-release -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" build size

deploy: compile
	idf.py -B build-release -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" flash monitor

clean:
	rm -rf build
	rm -rf build-release
	rm -rf sdkconfig
	rm -rf dependencies.lock

.PHONY: compile deploy clean
