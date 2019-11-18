export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

DATE_START=$(date +"%s")

make -j4
tools/dtbTool -2 -o arch/arm/boot/dtb -s 2048 -p scripts/dtc/ arch/arm/boot/

DATE_END=$(date +"%s")
DIFF=$(($DATE_END - $DATE_START))

echo "Time: $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
