diff --git a/test/roms/build.sh b/test/roms/build.sh
index e8256bcfd..61bd7a7d2 100755
--- a/test/roms/build.sh
+++ b/test/roms/build.sh
@@ -24,7 +24,7 @@ if ! command -v "$SJASMPLUS" >/dev/null 2>&1; then
 	exit 1
 fi
 
-for base in msx_60hz_16kb_ldir msx_60hz_16kb_ldir_loop msx_60hz_16kb_otir msx_60hz_16kb_ldir_sub; do
+for base in msx_60hz_16kb_ldir msx_60hz_16kb_ldir_loop msx_60hz_16kb_otir msx_60hz_16kb_ldir_sub msx_60hz_16kb_ldir_nested; do
 	echo "building $base.rom"
 	"$SJASMPLUS" --raw="$base.rom" "$base.asm" >/dev/null
 done
