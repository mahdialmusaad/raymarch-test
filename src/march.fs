#version 330 core

out vec4 frag;
in vec3 localpos;

uniform float time;
uniform vec3 campos;
uniform mat4 model;

float mod289(float x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 perm(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }
float noise(vec3 p)
{
	vec3 a = floor(p);
	vec3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 k1 = perm(b.xyxy);
	vec4 k2 = perm(k1.xyxy + b.zzww);

	vec4 c = k2 + a.zzzz;
	vec4 k3 = perm(c);
	vec4 k4 = perm(c + 1.0);

	vec4 o1 = fract(k3 * (1.0 / 41.0));
	vec4 o2 = fract(k4 * (1.0 / 41.0));

	vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	return o4.y * d.y + o4.x * (1.0 - d.y);
}

void main()
{
	vec3 origin = (inverse(model) * vec4(campos, 1.0)).xyz + vec3(0.5);
	vec3 dir = normalize(localpos - origin);

	vec3 inv = 1.0 / (dir + 1e-6);
	vec3 tbot = inv * -origin;
	vec3 ttop = inv * (vec3(1.0) - origin);
	
	vec3 tmin = min(tbot, ttop);
	vec3 tmax = max(tbot, ttop);
	float tnear = max(max(tmin.x, tmin.y), tmin.z);
	float tfar = min(min(tmax.x, tmax.y), tmax.z);

	if (!(tnear < tfar && tfar > 0.0)) discard;
	if (tnear < 0.0) tnear = 0.0;

	vec3 current = origin + dir * tnear;
	vec4 col = vec4(0.0);
	float dist = tnear;

	for (int i = 0; i < 256; i++) {
		if (dist > tfar || col.a >= 0.95) break;
		float density = noise(current + vec3(time));
		if (density < 0.5) {
			float dim = density * 0.4;
			float ainv = 1.0 - col.a;
			col.rgb += vec3(density * dim) * ainv;
			col.a += dim * ainv;
		}
		current += dir * 0.005;
		dist += 0.005;
	}

	if (col == 0.0) discard;
	frag = vec4(col);
}
