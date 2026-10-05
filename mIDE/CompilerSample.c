// mIDE compiler feature sample.
// This demonstrates the currently supported C-like subset:
// functions, scalar type spellings, qualifiers, const, booleans, comparisons,
// conditionals, loops, arithmetic optimization, and Mindustry built-ins.

extern display display1;

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

	int counter = 0;
	while (counter < 3) {
		score = score + counter;
		counter = counter + 1;
	}

	for (int index = 0; index < 2; index = index + 1) {
		score = score + index;
	}

	if (score > 0) {
		score = score + 1;
	} else {
		score = score - 1;
	}

	print("score");
	print(score);
	drawflush(display1);
	print(larger);
	print(same);
	print(ratio);
	return score;
}
