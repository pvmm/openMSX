# Prints which 3rd party libraries are desired for the given configuration.

from components import requiredLibrariesFor
from configurations import getConfiguration
from libraries import allDependencies, librariesByName
from packages import iterDownloadablePackages

def main(platform, linkMode):
	configuration = getConfiguration(linkMode)
	components = configuration.iterDesiredComponents()

	# Compute the set of all directly and indirectly required libraries,
	# then filter out system libraries.
	thirdPartyLibs = set(
		makeName
		for makeName in allDependencies(requiredLibrariesFor(components))
		if not librariesByName[makeName].isSystemLibrary(platform)
		)

	# ALSA exists on Linux only.
	# While Android has the necessary kernel interfaces, the lib doesn't
	# support Android.
	if platform != 'linux':
		thirdPartyLibs.discard('ALSA')

	print ' '.join(sorted(thirdPartyLibs))

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 3:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python 3rdparty_libraries.py TARGET_OS LINK_MODE'
			)
		sys.exit(2)
