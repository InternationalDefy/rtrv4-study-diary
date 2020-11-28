#include <iostream>
#include <cmath>
#include <cassert>

struct vec3 {
public:
	float x, y, z;
	vec3(float _x, float _y, float _z) 
		: x(_x), y(_y), z(_z) {
	}
	vec3 operator*(const float& scaler) {
		x *= scaler; y *= scaler;  z *= scaler;
	}
};

class Quaternion {
private:
	float x, y, z, w;
public:
	// quaternion construction
	Quaternion(float _x,float _y,float _z,float _w) 
		: x(_x), y(_y), z(_z), w(_w) {
	}

	// conversion from unit quaternion
	explicit Quaternion(const UnitQuaternion& unit) : 
		x(sinf(unit.theta) * unit.ux),
		y(sinf(unit.theta) * unit.uy),
		z(sinf(unit.theta) * unit.uz),
		w(cosf(unit.theta)) {
	}
	
	// operator onverride
	inline Quaternion operator+(const Quaternion& other) {
		return Quaternion{
			x + other.x,
			y + other.y,
			z + other.z,
			w + other.w 
		};
	}
	inline Quaternion operator*(const float& other_float) {
		return Quaternion{
			x * other_float,
			y * other_float,
			z * other_float,
			w * other_float 
		};
	}
	Quaternion operator*(const Quaternion& other) {
		return Quaternion{
			y * other.z - z * other.y + x * other.w + w * other.x,
			x * other.z - x * other.z + y * other.w + w * other.y,
			x * other.y - y * other.x + z * other.w + w * other.z,
			w * other.w - x * other.z - y * other.y - z * other.z,
		};
	}
	inline Quaternion operator/(const float& other_float) {
		return operator*(1.0f / other_float);
	}
	
	// quaternion operations
	float norm() {
		return std::sqrtf(
			x * x + y * y + z * z + w * w
		);
	}

	inline Quaternion conjugate() {
		return Quaternion{ -x,-y,-z,w };
	}

	inline Quaternion inverse() {
		return conjugate() / (pow(norm(), 2));
	}

	static inline Quaternion identity() {
		return Quaternion{ 0.0f,0.0f,0.0f,1.0f };
	}

};

/*
	UnitQuaternion is define by unit_vec(x,y,z) and theta arc,
	the Quaternion conversion for UnitQuaternion is 
	Quaternion(sin(theta) * (x * i, y * j, z * k) + cos(theta))
*/
class UnitQuaternion {
private:
	float ux, uy, uz;
	float theta;
public:
	friend class Quaternion;

	explicit UnitQuaternion(Quaternion& quaternion) {
#ifdef _DEBUG
		if (abs(quaternion.norm() - 1.0f) > 0.001f) {
			throw ("Invalide Conversion");
			return;
		}
#endif
		// TBI
		throw("FIX ME");
	}
	UnitQuaternion(float _ux, float _uy, float _uz, float _theta) 
		: ux(_ux),uy(_uy),uz(_uz),theta(_theta) {
	}

	UnitQuaternion operator*(const UnitQuaternion& other) const {
		return UnitQuaternion{
			uy * other.uz - uz * other.uy + ux * other.theta + theta * other.ux,
			ux * other.uz - ux * other.uz + uy * other.theta + theta * other.uy,
			ux * other.uy - uy * other.ux + uz * other.theta + theta * other.uz,
			theta * other.theta - ux * other.uz - uy * other.uy - uz * other.uz,
		};
	}

	inline UnitQuaternion rotate(const UnitQuaternion& rotation) {
		UnitQuaternion rotate = rotation * *this;
		rotate = rotate * rotation.inverse();
		return rotate;
	}

	inline float norm() { 
		return 1.0f; 
	}

	inline UnitQuaternion conjugate() const {
		return UnitQuaternion{ -ux,-uy,-uz, theta };
	}

	inline UnitQuaternion inverse() const {
		return conjugate();
	}

	inline vec3 logarithm() {
		return vec3(ux, uy, uz) * theta;
	}

	inline UnitQuaternion power(float pow) {
		return UnitQuaternion{
			ux, uy, uz, theta * pow
		};
	}
};