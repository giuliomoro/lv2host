#include <OnePole.h>
#include <math.h>
#include <stdio.h>

OnePole::OnePole() {}

OnePole::OnePole(float fc, int type)
{
	setup(fc, type);
}

int OnePole::setup(float fc, int type)
{
	setType(type);
	setFc(fc);

	return 0;
}

void OnePole::setFc(float fc)
{
	if(type_ == LP)
	{
		b1 = exp(-2.0 * M_PI * fc);
		a0 = 1.0 - b1;
	}
	else if(type_ == HP)
	{
		b1 = -exp(-2.0 * M_PI * (0.5 - fc));
		a0 = 1.0 + b1;
	}
	fc_ = fc;
}

void OnePole::setType(int type)
{
	if(type == LP || type == HP)
	{
		type_ = type;
	}	
	else
	{
		fprintf(stderr, "Unvalid type\n");
	}
}	

float OnePole::process(float input)
{
	return ym1 = input * a0 + ym1 * b1;
}

OnePole::~OnePole()
{
	cleanup();
}

void OnePole::cleanup() { }
