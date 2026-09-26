// mlog-compatible compiler stress sample.
// Supported constructs only: int functions, assignments, arithmetic, and return.

int square(int value) {
	return value * value;
}

int cube(int value) {
	return value * value * value;
}

int checksum(int a, int b, int c, int d) {
	int result = a * 3;
	result = result + b * 5;
	result = result + c * 7;
	result = result + d * 11;
	result = result + a * a;
	result = result - b * b;
	result = result + c * c;
	result = result - d * d;
	return result;
}

int transformA(int value) {
	int result = value * 3;
	result = result + 7;
	result = result - 19;
	result = result * result;
	result = result / 3;
	return result;
}

int transformB(int value) {
	int result = value * 5;
	result = result - 13;
	result = result + 91;
	result = result / 2;
	result = result * 7;
	return result;
}

int transformC(int value) {
	int result = value + 100;
	result = result * 2;
	result = result - 33;
	result = result / 5;
	return result;
}

int main() {
	int seed = 7;
	int doubled = seed * 2;
	int offset = doubled + 5;
	int grouped = (offset + 3) * 4;
	int quotient = grouped / 2;
	int result = quotient - seed;

	seed = seed + 1;
	doubled = seed * seed;
	offset = doubled - 10;
	grouped = (offset * 2) + (seed + 3);
	quotient = grouped / 3;
	result = result + quotient;

	int a = 2 + 3 * 4;
	int b = (2 + 3) * 4;
	int c = a + b / 2;
	int d = (c - a) * (b - 1);
	result = result + d;

	int step01 = result + 11;
	int step02 = step01 * 2;
	int step03 = step02 - 7;
	int step04 = step03 / 3;
	int step05 = (step04 + 9) * 2;
	int step06 = step05 - (seed * 2);
	int step07 = step06 / 4;
	int step08 = step07 + (a * 3);
	int step09 = (step08 - b) * 2;
	int step10 = step09 / 5;
	int step11 = step10 + c;
	int step12 = (step11 + d) / 2;
	int step13 = step12 * (seed + 1);
	int step14 = step13 - grouped;
	int step15 = step14 / 3;

	step01 = step15 + result;
	step02 = step01 * step01;
	step03 = step02 / (seed + 2);
	step04 = step03 - step12;
	step05 = (step04 + step08) * 2;
	step06 = step05 / 7;
	step07 = step06 + step10;
	step08 = step07 - step11;
	step09 = (step08 * 3) + step13;
	step10 = step09 / 2;
	step11 = step10 - step14;
	step12 = (step11 + step15) * 4;
	step13 = step12 / 6;
	step14 = step13 + step02;
	step15 = step14 - step03;

	int value01 = checksum(seed, a, b, c);
	int value02 = checksum(value01, step01, step02, step03);
	int value03 = transformA(value02);
	int value04 = transformB(value03);
	int value05 = transformC(value04);
	int value06 = square(value05);
	int value07 = cube(seed);
	int value08 = value06 + value07;
	int value09 = checksum(value05, value06, value07, value08);
	int value10 = transformA(value09);

	int total01 = result + step01;
	int total02 = total01 + step05;
	int total03 = total02 + step09;
	int total04 = total03 + step13;
	int total05 = total04 + step15;
	int scaled01 = total05 * 2;
	int scaled02 = scaled01 / 3;
	int scaled03 = scaled02 - seed;
	int finalValue = (scaled03 + a + b) / 2;

	finalValue = finalValue + value10;
	finalValue = finalValue + value09;
	finalValue = finalValue / 2;
	return cube(finalValue);
}
