echo "total code lines:"
find . -type f -name "*.[hc]" | xargs cat | wc -l
echo "test code lines:"
find */ -type f -name "*.[hc]" | xargs cat | wc -l
