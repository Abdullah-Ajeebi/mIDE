// mIDE compiler feature sample.
// This demonstrates the currently supported C-like subset:
// functions, scalar type spellings, qualifiers, const, booleans, comparisons,
// arithmetic optimization, and Mindustry built-ins.

inline int square(const int value) {
	return value * value;
}

static long weighted(int value, short weight) {
	int result = value * weight;
	result = result + 2 * 3;
	return result;
}

int main() {
	const int scale = 3;
	auto int seed = 7;
	register unsigned int doubled = seed * 2;
	signed int offset = doubled + scale;
	char smallValue = 4;
	float ratio = 2.5;
	bool enabled = true;
	bool larger = doubled > seed;
	bool same = seed == 7;

	// These assignments exercise the second optimizer pass.
	int folded = 1 + 2;
	folded = folded * 10;
	folded = folded - 5;

	// Unused is removed by the dead-store pass.
	int unused = 88;

	int score = square(offset) + weighted(smallValue, 5);
	score = score + folded;
	score = score + enabled;

	print("score");
	print(score);
	print(larger);
	print(same);
	print(ratio);
	return score;
}
