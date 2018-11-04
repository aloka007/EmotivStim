
double gaussian(double x, double b){
	double a = 1.0, c = 10.0;

	return pow((a * 2.71828), (-(pow((x - b), 2)) / (2 * pow(c, 2))));
}

double weighting(double AF4, double P7, double O1, double O2){
	double w1 = 0.2, w2 = 1.0, w3 = 0.6, w4 = 0.6;

	AF4 = (AF4 > 0) ? AF4 : 0;
	P7 = (P7 < 0) ? P7 : 0;
	O1 = (O1 < 0) ? O1 : 0;
	O2 = (O2 < 0) ? O2 : 0;

	return (AF4*w1 - P7*w2 - O1*w3 - O2*w4);
}