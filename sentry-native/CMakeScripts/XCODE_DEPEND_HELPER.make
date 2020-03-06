# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.example.Debug:
PostBuild.sentry.Debug: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/example:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example


PostBuild.example_crashpad.Debug:
PostBuild.sentry.Debug: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad


PostBuild.sentry.Debug:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a


PostBuild.sentry_test_integration.Debug:
PostBuild.sentry.Debug: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration


PostBuild.sentry_test_unit.Debug:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_unit:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_unit


PostBuild.example.Release:
PostBuild.sentry.Release: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/example:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example


PostBuild.example_crashpad.Release:
PostBuild.sentry.Release: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/example_crashpad


PostBuild.sentry.Release:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a


PostBuild.sentry_test_integration.Release:
PostBuild.sentry.Release: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_integration


PostBuild.sentry_test_unit.Release:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_unit:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/sentry_test_unit


PostBuild.example.MinSizeRel:
PostBuild.sentry.MinSizeRel: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example


PostBuild.example_crashpad.MinSizeRel:
PostBuild.sentry.MinSizeRel: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example_crashpad
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example_crashpad:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/example_crashpad


PostBuild.sentry.MinSizeRel:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a


PostBuild.sentry_test_integration.MinSizeRel:
PostBuild.sentry.MinSizeRel: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/sentry_test_integration
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/sentry_test_integration:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/sentry_test_integration


PostBuild.sentry_test_unit.MinSizeRel:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/sentry_test_unit:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/sentry_test_unit


PostBuild.example.RelWithDebInfo:
PostBuild.sentry.RelWithDebInfo: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example


PostBuild.example_crashpad.RelWithDebInfo:
PostBuild.sentry.RelWithDebInfo: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example_crashpad
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example_crashpad:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/example_crashpad


PostBuild.sentry.RelWithDebInfo:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a


PostBuild.sentry_test_integration.RelWithDebInfo:
PostBuild.sentry.RelWithDebInfo: /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/sentry_test_integration
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/sentry_test_integration:\
	/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/sentry_test_integration


PostBuild.sentry_test_unit.RelWithDebInfo:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/sentry_test_unit:
	/bin/rm -f /Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/sentry_test_unit




# For each target create a dummy ruleso the target does not have to exist
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/MinSizeRel/libsentry.a:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/RelWithDebInfo/libsentry.a:
/Users/bsergeant/src/foss/IXWebSocket/sentry-native/libsentry.a:
