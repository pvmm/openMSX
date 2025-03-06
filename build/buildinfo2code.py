from cpu import getCPU, X86, X86_64
from makeutils import extractMakeVariables, parseBool
from outpututils import rewriteIfChanged

from os.path import dirname, join as joinpath
import sys

def iterBuildInfoHeader(targetPlatform, cpuName, flavour, installShareDir, installDocDir):
	platformVars = extractMakeVariables(
        joinpath(dirname(__file__), 'platform-%s.mk' % targetPlatform),
		dict.fromkeys(
			('COMPILE_FLAGS', 'LINK_FLAGS', 'LDFLAGS', 'TARGET_FLAGS',
				'OPENMSX_TARGET_CPU'),
			''
			)
		)
	setWindowIcon = parseBool(platformVars.get('SET_WINDOW_ICON', 'true'))

	targetCPU = getCPU(cpuName)

	# TODO: Add support for device-specific configuration.
	platformPandora = targetPlatform == 'pandora'
	platformAndroid = targetPlatform == 'android'

	# Defaults.
	minScaleFactor = 2
	maxScaleFactor = 4

	# Platform overrides.
	if platformAndroid:
		# At the moment, libSDL android crashes when trying to dynamically change the scale factor
		# TODO: debug why it crashes and then change the maxScaleFactor parameter here
		# so that people with a powerful enough android device can use a higher scale factor
		minScaleFactor = 2
		maxScaleFactor = 2
	elif platformPandora:
		maxScaleFactor = 3

	yield '// Automatically generated by build process.'
	yield ''
	yield '#ifndef BUILD_INFO_HH'
	yield '#define BUILD_INFO_HH'
	yield ''
	# Use a macro iso integer because we really need to exclude code sections
	# based on this.
	yield '#define PLATFORM_ANDROID %d' % platformAndroid
	yield '#define MIN_SCALE_FACTOR %d' % minScaleFactor
	yield '#define MAX_SCALE_FACTOR %d' % maxScaleFactor
	yield ''
	yield 'namespace openmsx {'
	yield ''
	yield 'static const bool OPENMSX_SET_WINDOW_ICON = %s;' \
		% str(setWindowIcon).lower()
	yield 'static const char* const DATADIR = "%s";' % installShareDir
	yield 'static const char* const DOCDIR = "%s";' % installDocDir
	yield 'static const char* const BUILD_FLAVOUR = "%s";' % flavour
	yield 'static const char* const TARGET_PLATFORM = "%s";' % targetPlatform
	yield 'static const char* const TARGET_CPU = "%s";' % targetCPU.name
	yield ''
	yield '} // namespace openmsx'
	yield ''
	yield '#endif // BUILD_INFO_HH'

if __name__ == '__main__':
	if len(sys.argv) == 7:
		rewriteIfChanged(sys.argv[1], iterBuildInfoHeader(*sys.argv[2 : ]))
	else:
		print(
			'Usage: python3 buildinfo2code.py CONFIG_HEADER '
			'platform cpu flavour share-install-dir doc-install-dir',
			file=sys.stderr
			)
		sys.exit(2)
