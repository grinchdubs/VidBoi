uniform vec4 color;
uniform vec2 scale;
uniform vec2 centre;
uniform vec2 inputVal;
uniform vec2 resolution;
uniform float cv0;
uniform float cv1;
uniform float cv2;
uniform float cv6;
uniform float cv7;
varying vec2 tcoord;
uniform float time;
uniform int sceneIndex;

uniform sampler2D texFB;
uniform sampler2D texCV;
uniform sampler2D texIN;

varying vec4 outColor;

#define PI 3.14159265358979323846
#define TWO_PI 6.28318530718

#define repeat(v) mod(p + 1., 2.) -1.
#define un(a, b) min(a, b)

float random (float x) {
    return fract(sin(x)*1e4);
}

highp float random(vec2 co)
{
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt= dot(co.xy ,vec2(a,b));
    highp float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}


// Color Space Conversion \-----------------------------|
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


//-------------------------------------------------|

vec2 rotate2D(vec2 _st, float _angle){
    _st -= 0.5;
    _st =  mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle)) * _st;
    _st += 0.5;
    return _st;
}

vec3 box(vec2 _st, vec2 _size, float _smoothEdges){
    _size = vec2(0.5)-_size*0.5;
    vec2 aa = vec2(_smoothEdges*0.5);
    vec2 uv = smoothstep(_size,_size+aa,_st);
    uv *= smoothstep(_size,_size+aa,vec2(1.0)-_st);
    return vec3(uv.x*uv.y);
}

float circle(vec2 _st, float _radius){
    vec2 l = _st-vec2(0.5);
    return 1.-smoothstep(_radius-(_radius*0.01),
                         _radius+(_radius*0.01),
                         dot(l,l)*4.0);
}

float circle(vec2 _st, float _radius, float thickness){
	return circle(_st, _radius) - circle(_st, _radius - thickness);
}

vec3 horizontalLine(vec2 st, float pos, float size, vec3 color){
	st.x = fract(st.x) - pos ;

	return (step(0., st.x  ) * 1.0 - step( size,  st.x  )) * color;
}

vec3 verticalLine(vec2 st, float pos, float size, vec3 color){
	st.y = fract(st.y) - pos ;
	return (step(0., st.y  ) * 1.0 - step( size,  st.y  )) * color;
}

vec3 polygon(vec2 st, vec2 position, float scale, float rotation,  float r, vec3 color){
	
	st = st + position;
	
	st = rotate2D( st, PI * rotation ) ;
	
	float d = 0.0;
	// Remap the space to -1. to 1.
	st = st *2.-1.;


  // Angle and radius from the current pixel
	float a = atan(st.x,st.y)+ PI ;
  
  
  // Shaping function that modulate the distance
  	d = cos(floor(.5+a/r)*r-a) * length(st) ;
	
  	return vec3(1.0-smoothstep(.40 , .405 , d) ) * color;
	
}

#define TRIR 2.09439510239
vec3 triangle(vec2 st, vec2 position, float scale, float rotation, vec3 color ){
	return polygon(st, position, scale, rotation,  TWO_PI/(4. *cv2), color);
}

vec3 hexagon(vec2 st, vec2 position, float scale, float rotation, vec3 color ){
	return polygon(st, position, scale, rotation,  TWO_PI/6., color);
}

void etchMain(){
	vec3 center = vec3(cv0 /0.5 ,cv1/0.5, cv2/0.5);
	vec2 pos = vec2(tcoord.x, tcoord.y)   ;

	
	float offset = 0.5 ;
	vec2 size = vec2(cv1 +.01 , cv2 +.01) / cv0;
	
	vec3 linecolor = vec3(0.5, cv1, cv2) ;
	
	vec3 texColor = texture2D( texFB, vec2(pos.x*2., pos.y*2.) ).xyz;
	
	vec3 color = vec3(horizontalLine(pos, cv0, 0.05  , linecolor)  + verticalLine( pos, 0., 0.05 , linecolor));
	
	//TODO: feedback needs to be fixed (currently only feeding back a fraction of the resulting buffer when dividing rendering context)
	if (texColor.x > 0.8 || texColor.y >.8 || texColor.z > .8){
		if(cv2 > 0.01){
			color = color + texColor * (0.87 + cv2 * 0.10);
		}
	}

	//~ vec3 color = vec3(box(pos , size, .0) + center)  - vec3( circle(pos , cv0 ) ) ;

	
	gl_FragColor = vec4( color, 1.0 );
}

void geometricMain(){

	vec2 st = vec2(tcoord.x, tcoord.y);
	
	//exp
	st = st *2.-1.;


  // Angle and radius from the current pixel
	float ang = atan(st.x,st.y) + PI  ;
	float rad = sqrt(dot(st,st));
  
	ang = cos(ang) + (tcoord.x * (cv2-0.5) * 2.);
	rad = rad * sin(cv1 * PI/2. );
	
	st = fract(vec2(0.1/rad, ang/PI) * (10. * cv0));

	//end exp
	//~ vec2 ipos = floor(st);
	//~ vec2 cpos = vec2( floor(tileN/2.) );
	//~ vec2 toCenterCPos = ipos - cpos; // vector ( in cell space) to the center cell
	//~ float dCtrPos= distance(ipos, cpos);
	
	vec3 color = vec3(0.247058823529, 0.21568627451, 0.18431372549);
  	
	//~ st = rotate2D( st, PI * time/2. ) ;
	//vec3(st, 0);//
	vec3 colorIn = mix(vec3(0.972549019608 ,0.776470588235,0.349019607843), vec3( 0.9568627451, 0.952941176471, 0.854901960784), cv0)  ;
	vec3 triColor =triangle(st, vec2(0.0), .2  , cv1 ,  colorIn);
 	color =  max(triColor, color); //+  texture2D( texIN, st).xyz ;

  	gl_FragColor = vec4(color,1.0);
}

vec3 drawImage(vec2 st, vec2 position, float scale){
	
	st.y = 1. - st.y;
	st.x -= position.x;
	st.y += 1.- position.y - 1.;
	vec2 texSt = (st *scale);
	
	//~ if(texSt.x > 1. || texSt.x < 0. || texSt.y > 1. || texSt.y < 0.){
		//~ return vec3(0,0,0);
	//~ }
	
	vec3 color = texture2D( texIN, texSt).xyz;
	
	if(color == vec3(1.,1.,1.)){
		return vec3(0.);
	}
	
	color = vec3(color.x * cv0, color.y * cv1, color.z * cv2);
	
	
	
	return color;
}

void vaporMain(){
	vec2 st = vec2(tcoord.x, tcoord.y) * 2. - 1.;
	//~ st.y += 0.5;
	float ang = atan(st.x,st.y) + PI ;
	float rad = sqrt(dot(st,st));
	
	//rad += sin(7.85 + ang);

		//vec3(.9725,0.776470588235, 0.835294117647);
	vec3 color = vec3(sin(time/95.)  , cos(time/82.) , cos(time/90.) );
	float sometime = sin(time/92.);
	float sometime2 = sin(time/72.);
	color += vec3( rad - cv0, rad - cv1, rad - cv2) ;
	color += vec3(sin(rad) + (-1. * cv0) + log(rad + sometime), 
	sin(rad - sin(time/52.)) + (-1. * cv1) + log(rad + sometime2) , 
	sin(rad - + sin(time/32.)) + (-1. * cv2) + log(rad + sin(time/32.)) ) ;
	//~ color.x += clamp(cos(ang * (100.) - sometime ), 0.4 , .45 - (cv0 *0.5));
	//~ color.y += clamp(cos(ang * (100.)  + sometime2 ), 0.4, .45 - (cv0 *0.5));
	//~ color.y += clamp(cos(ang * (100.)  - sometime2 ), 0.4, .45 - (cv0 *0.5));
	



	float offset = .1;
	vec3 imgColor = drawImage(vec2(log(rad) + sometime2, ang ), vec2(ang, ang), .8);
	//~ if(imgColor != vec3(0,0,0)){
		//~ color += imgColor;
	//~ }
	color += imgColor;
	
	gl_FragColor = vec4(color, 1.0);
}

vec2 toPolar(vec2 st){ // Outputs distance 0 to 1 and Thet
	st = st *2.-1.;

  // Angle and radius from the current pixel
	float ang = ( atan(st.y,st.x) + PI ) / TWO_PI; // angle is scaled so you can use this to adress stuff and be more shader like
	float rad = sqrt(dot(st,st)) ;

	st = vec2(ang, rad);
  
	// ang = cos(ang) + (tcoord.x * (cv2-0.5) * 2.);
	// rad = rad * sin(cv1 * PI/2. );
	
	// st = fract(vec2(0.1/rad, ang/PI) * (10. * cv0));

	return st; 
}

mat3 rotateX(float a) {
	return mat3(
    	1.0, 0.0, 0.0,
        0.0, cos(a), sin(a),
        0.0, -sin(a), cos(a)
    );
}

mat3 rotateY(float a) {
	return mat3(
    	cos(a), 0.0, sin(a),
        0.0, 1.0, 0.0,
        -sin(a), 0.0, cos(a)
    );
}

float sphere_sdf(vec3 p, float r) {
	return length(p) - r;
}

float cube_sdf(vec3 p, float s) {
	return length(max(abs(p) - s, .0));
}

float ring_sdf(vec3 p) {
	float a = sphere_sdf(p + vec3(.2, .0, .0), .1);
    float b = sphere_sdf(p + vec3(-.2, .0, .0), .1);
    float A = un(a, b);
    float c = sphere_sdf(p + vec3(.0, .0, .1), .1);
    float d = sphere_sdf(p + vec3(.0, .0, -.1), .1);
    float B = un(c, d);
    return un(A, B);
}

float shape_sdf(vec3 p) {
    vec3 v = rotateY(time) * p;
    v.y = mod(v.y + 0.2, 0.4) - 0.2;
    return sphere_sdf(v, .1);//ring_sdf(v);//un(, sphere_sdf(p * vec3(1., .01, 1.), .11));
    
}


//Color Quantizer ( Color Tables? )
vec3 pallete(float root){

	vec2 triad = vec2(0.33333333, 0.66666666);

	vec3 ret = vec3( root, root + triad[0], root +  triad[1] ) ;

	return ret; //return a quantized color scheme
}

vec3 colorizer( float amplitude,  float color, float saturation ){
	float value = (amplitude - 0.5) * 2.; //take out this math( input voltages should be unipolar )

	return hsv2rgb( vec3(color, saturation, value) );
}

void datBoiFrag(){
	vec3 texColor = vec3(0.);

	//vec3 center = vec3(cv0 /0.5 ,cv1/0.5, cv2/0.5);
	vec2 pos = vec2(tcoord.x, tcoord.y) ;
	vec2 swappedPos = vec2( tcoord.y, tcoord.x);

	//Rotation control for UV in (TODO: rotation per osc)
	vec2 rotationPos = rotate2D(pos, clamp(cv7*1.2-0.1, 0., 1.) * -TWO_PI);

	vec2 polarPos = toPolar(rotationPos) ;
	vec2 swappedPolar = vec2( polarPos.y  , polarPos.x );
	polarPos = vec2( sin(polarPos.x * PI ) , polarPos.y  ) ;

	vec2 texPos = mix(rotationPos, polarPos, clamp(cv0*1.1, 0., 1.)  );

	texColor = texture2D( texCV, texPos ).xyz;


	//Color Quantization(?) / Palette Selector

	vec3 hue = pallete(cv6 * .6666);


	//OSC
	vec3 oscA = colorizer(texColor.x, hue[0], cv1 );
	vec3 oscB = colorizer(texColor.y, hue[1],  cv1  );
	vec3 oscC = colorizer(texColor.z, hue[2],  cv1 );
	

	//Feedback
	vec3 fbColor = texture2D( texFB, pos ).xyz  * cv2 * 2.;


	//Mixer
	vec3 color = (oscA  +  oscB  + oscC ) + fbColor; //TODO: add oscC when hardware is done
	//--------

	gl_FragColor = vec4( color, 1.0 );
	
}

float osc(float scan, float frequency, float phase, float drift){
	return  (sin(scan * frequency + (drift * time) + phase ) );
}

void datBoiTest(){
 	vec3 texColor = texture2D( texCV, tcoord.xy ).xyz;

 	float scan = ((tcoord.x * 100.) + (tcoord.y * 100. ) * 100.) / 1000.  ;

 	float syncDrift = 0.5;
	
	float f1 = 100. * cv0;
	float f2 = 100. * cv1;
	float f3 = 100. * cv2;
	
	float osc1 = osc(scan, f1, 0., syncDrift) * 1.;
	float osc2 = osc(scan, f2, 0., syncDrift) * 1.;
	float osc3 = osc(scan, f3, 0., syncDrift) * 0.;

	gl_FragColor = vec4( vec3( osc1, osc2 , texColor.x ), 1.0 );
}

void main( void ) {
	datBoiFrag();
	//datBoiTest();

}
