/*
 * Adapted from: http://www.earlevel.com/main/2012/12/15/a-one-pole-filter/
 *
 */

class OnePole
{

	private:
		
		float fc_;
		int type_;

		float a0, b1, ym1;

	public:
		OnePole();
		OnePole(float fc, int type);
		~OnePole();
		
		int setup(float fc, int type = LP);
		void cleanup();
		
		enum Type 
		{
			LP = 0,
			HP = 1
		};
		
		void setFc(float fc);
		void setType(int type);
		
		float process(float input);
};
