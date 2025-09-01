#pragma once

#include "Core.h"

#include <math.h>

namespace Bk
{
	template<typename T>
	union TVec2
	{
		static const TVec2 Zero;
		static const TVec2 One;

		constexpr TVec2() = default;
		constexpr TVec2(T x, T y);

		constexpr T& operator[](size_t idx);
		constexpr T operator[](size_t idx) const;

		T elements[2];

		struct
		{
			T x, y;
		};
	};

	template<typename T>
	union TVec3
	{
		static const TVec3 Zero;
		static const TVec3 One;

		constexpr TVec3() = default;
		constexpr TVec3(T x, T y, T z);

		constexpr T& operator[](size_t idx);
		constexpr T operator[](size_t idx) const;

		T elements[3];

		struct
		{
			T x, y, z;
		};
	};

	template<typename T>
	union TVec4
	{
		static const TVec4 Zero;
		static const TVec4 One;

		constexpr TVec4() = default;
		constexpr TVec4(T x, T y, T z, T w);

		constexpr T& operator[](size_t idx);
		constexpr T operator[](size_t idx) const;

		T elements[4];

		struct
		{
			T x, y, z, w;
		};
	};

	template<typename T>
	union TQuat
	{
		static const TQuat Identity;

		constexpr TQuat() = default;
		constexpr TQuat(T x, T y, T z, T w);

		constexpr T& operator[](size_t idx);
		constexpr T operator[](size_t idx) const;

		T elements[4];

		struct
		{
			T x, y, z, w;
		};
	};

	template<typename T>
	union TMat4
	{
		static const TMat4 Identity;

		constexpr TMat4() = default;
		constexpr TMat4(TVec4<T> c0, TVec4<T> c1, TVec4<T> c2, TVec4<T> c3);

		constexpr TVec4<T>& operator[](size_t idx);
		constexpr TVec4<T> operator[](size_t idx) const;

		T elements[4][4];
		TVec4<T> columns[4];
	};

	using Vec2f = TVec2<float>;
	using Vec2d = TVec2<double>;
	using Vec2i = TVec2<int32>;
	using Vec2u = TVec2<uint32>;

	using Vec3f = TVec3<float>;
	using Vec3d = TVec3<double>;
	using Vec3i = TVec3<int32>;
	using Vec3u = TVec3<uint32>;

	using Vec4f = TVec4<float>;
	using Vec4d = TVec4<double>;
	using Vec4i = TVec4<int32>;
	using Vec4u = TVec4<uint32>;

	using Quat4f = TQuat<float>;
	using Quat4d = TQuat<double>;

	using Mat4f = TMat4<float>;
	using Mat4d = TMat4<double>;
}

namespace Bk
{
	inline float Sqrt(float value)
	{
		return sqrtf(value);
	}

	inline double Sqrt(double value)
	{
		return sqrt(value);
	}

	inline float InvSqrt(float value)
	{
		return 1.0f / sqrtf(value);
	}

	inline double InvSqrt(double value)
	{
		return 1.0 / sqrt(value);
	}

	inline float Abs(float value)
	{
		return fabsf(value);
	}

	inline double Abs(double value)
	{
		return fabs(value);
	}

	inline float Sin(float value)
	{
		return sinf(value);
	}

	inline double Sin(double value)
	{
		return sin(value);
	}

	inline float Cos(float value)
	{
		return cosf(value);
	}

	inline double Cos(double value)
	{
		return cos(value);
	}

	inline float Tan(float value)
	{
		return tanf(value);
	}

	inline double Tan(double value)
	{
		return tan(value);
	}

	template<typename T>
	const TVec2<T> TVec2<T>::Zero = { 0, 0 };

	template<typename T>
	const TVec2<T> TVec2<T>::One = { 1, 1 };

	template<typename T>
	constexpr TVec2<T>::TVec2(T x, T y)
		: x(x), y(y)
	{
	}

	template<typename T>
	constexpr T& TVec2<T>::operator[](size_t idx)
	{
		return elements[idx];
	}

	template<typename T>
	constexpr T TVec2<T>::operator[](size_t idx) const
	{
		return elements[idx];
	}

	template<typename T>
	constexpr TVec2<T> operator-(TVec2<T> a)
	{
		TVec2<T> result;
		result.x = -a.x;
		result.y = -a.y;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator+(TVec2<T> left, T right)
	{
		TVec2<T> result;
		result.x = left.x + right;
		result.y = left.y + right;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator+=(TVec2<T>& left, T right)
	{
		left.x = left.x + right;
		left.y = left.y + right;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator-(TVec2<T> left, T right)
	{
		TVec2<T> result;
		result.x = left.x - right;
		result.y = left.y - right;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator-=(TVec2<T>& left, T right)
	{
		left.x = left.x - right;
		left.y = left.y - right;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator*(TVec2<T> left, T right)
	{
		TVec2<T> result;
		result.x = left.x * right;
		result.y = left.y * right;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator*=(TVec2<T>& left, T right)
	{
		left.x = left.x * right;
		left.y = left.y * right;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator/(TVec2<T> left, T right)
	{
		TVec2<T> result;
		result.x = left.x / right;
		result.y = left.y / right;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator/=(TVec2<T>& left, T right)
	{
		left.x = left.x / right;
		left.y = left.y / right;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator+(TVec2<T> left, TVec2<T> right)
	{
		TVec2<T> result;
		result.x = left.x + right.x;
		result.y = left.y + right.y;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator+=(TVec2<T>& left, TVec2<T> right)
	{
		left.x = left.x + right.x;
		left.y = left.y + right.y;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator-(TVec2<T> left, TVec2<T> right)
	{
		TVec2<T> result;
		result.x = left.x - right.x;
		result.y = left.y - right.y;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator-=(TVec2<T>& left, TVec2<T> right)
	{
		left.x = left.x - right.x;
		left.y = left.y - right.y;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator*(TVec2<T> left, TVec2<T> right)
	{
		TVec2<T> result;
		result.x = left.x * right.x;
		result.y = left.y * right.y;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator*=(TVec2<T>& left, TVec2<T> right)
	{
		left.x = left.x * right.x;
		left.y = left.y * right.y;

		return left;
	}

	template<typename T>
	constexpr TVec2<T> operator/(TVec2<T> left, TVec2<T> right)
	{
		TVec2<T> result;
		result.x = left.x / right.x;
		result.y = left.y / right.y;

		return result;
	}

	template<typename T>
	constexpr TVec2<T> operator/=(TVec2<T>& left, TVec2<T> right)
	{
		left.x = left.x / right.x;
		left.y = left.y / right.y;

		return left;
	}

	template<typename T>
	constexpr bool operator==(TVec2<T> left, TVec2<T> right)
	{
		return left.x == right.x && left.y == right.y;
	}

	template<typename T>
	constexpr T LengthSq(TVec2<T> a)
	{
		return a.x * a.x + a.y * a.y;
	}

	template<typename T>
	constexpr T Length(TVec2<T> a)
	{
		return Sqrt(LengthSq(a));
	}

	template<typename T>
	constexpr TVec2<T> Normalize(TVec2<T> a)
	{
		T lengthSq = LengthSq(a);
		if (lengthSq > 0)
		{
			return a * InvSqrt(lengthSq);
		}

		return TVec2<T>::Zero;
	}

	template<typename T>
	constexpr TVec2<T> Lerp(TVec2<T> a, TVec2<T> b, T t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	constexpr T Dot(TVec2<T> left, TVec2<T> right)
	{
		return left.x * right.x + left.y * right.y;
	}

	template<typename T>
	const TVec3<T> TVec3<T>::Zero = { 0, 0, 0 };

	template<typename T>
	const TVec3<T> TVec3<T>::One = { 1, 1, 1 };

	template<typename T>
	constexpr TVec3<T>::TVec3(T x, T y, T z)
		: x(x), y(y), z(z)
	{
	}

	template<typename T>
	constexpr T& TVec3<T>::operator[](size_t idx)
	{
		return elements[idx];
	}

	template<typename T>
	constexpr T TVec3<T>::operator[](size_t idx) const
	{
		return elements[idx];
	}

	template<typename T>
	constexpr TVec3<T> operator-(TVec3<T> a)
	{
		TVec3<T> result;
		result.x = -a.x;
		result.y = -a.y;
		result.z = -a.z;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator+(TVec3<T> left, T right)
	{
		TVec3<T> result;
		result.x = left.x + right;
		result.y = left.y + right;
		result.z = left.z + right;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator+=(TVec3<T>& left, T right)
	{
		left.x = left.x + right;
		left.y = left.y + right;
		left.z = left.z + right;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator-(TVec3<T> left, T right)
	{
		TVec3<T> result;
		result.x = left.x - right;
		result.y = left.y - right;
		result.z = left.z - right;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator-=(TVec3<T>& left, T right)
	{
		left.x = left.x - right;
		left.y = left.y - right;
		left.z = left.z - right;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator*(TVec3<T> left, T right)
	{
		TVec3<T> result;
		result.x = left.x * right;
		result.y = left.y * right;
		result.z = left.z * right;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator*=(TVec3<T>& left, T right)
	{
		left.x = left.x * right;
		left.y = left.y * right;
		left.z = left.z * right;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator/(TVec3<T> left, T right)
	{
		TVec3<T> result;
		result.x = left.x / right;
		result.y = left.y / right;
		result.z = left.z / right;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator/=(TVec3<T>& left, T right)
	{
		left.x = left.x / right;
		left.y = left.y / right;
		left.z = left.z / right;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator+(TVec3<T> left, TVec3<T> right)
	{
		TVec3<T> result;
		result.x = left.x + right.x;
		result.y = left.y + right.y;
		result.z = left.z + right.z;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator+=(TVec3<T>& left, TVec3<T> right)
	{
		left.x = left.x + right.x;
		left.y = left.y + right.y;
		left.z = left.z + right.z;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator-(TVec3<T> left, TVec3<T> right)
	{
		TVec3<T> result;
		result.x = left.x - right.x;
		result.y = left.y - right.y;
		result.z = left.z - right.z;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator-=(TVec3<T>& left, TVec3<T> right)
	{
		left.x = left.x - right.x;
		left.y = left.y - right.y;
		left.z = left.z - right.z;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator*(TVec3<T> left, TVec3<T> right)
	{
		TVec3<T> result;
		result.x = left.x * right.x;
		result.y = left.y * right.y;
		result.z = left.z * right.z;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator*=(TVec3<T>& left, TVec3<T> right)
	{
		left.x = left.x * right.x;
		left.y = left.y * right.y;
		left.z = left.z * right.z;

		return left;
	}

	template<typename T>
	constexpr TVec3<T> operator/(TVec3<T> left, TVec3<T> right)
	{
		TVec3<T> result;
		result.x = left.x / right.x;
		result.y = left.y / right.y;
		result.z = left.z / right.z;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> operator/=(TVec3<T>& left, TVec3<T> right)
	{
		left.x = left.x / right.x;
		left.y = left.y / right.y;
		left.z = left.z / right.z;

		return left;
	}

	template<typename T>
	constexpr bool operator==(TVec3<T> left, TVec3<T> right)
	{
		return left.x == right.x && left.y == right.y && left.z == right.z;
	}

	template<typename T>
	constexpr T LengthSq(TVec3<T> a)
	{
		return a.x * a.x + a.y * a.y + a.z * a.z;
	}

	template<typename T>
	constexpr T Length(TVec3<T> a)
	{
		return Sqrt(LengthSq(a));
	}

	template<typename T>
	constexpr TVec3<T> Normalize(TVec3<T> a)
	{
		T lengthSq = LengthSq(a);
		if (lengthSq > 0)
		{
			return a * InvSqrt(lengthSq);
		}

		return TVec3<T>::Zero;
	}

	template<typename T>
	constexpr TVec3<T> Lerp(TVec3<T> a, TVec3<T> b, T t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	constexpr T Dot(TVec3<T> left, TVec3<T> right)
	{
		return left.x * right.x + left.y * right.y + left.z * right.z;
	}

	template<typename T>
	constexpr TVec3<T> Cross(TVec3<T> left, TVec3<T> right)
	{
		TVec3<T> result;
		result.x = left.y * right.z - left.z * right.y;
		result.y = left.z * right.x - left.x * right.z;
		result.z = left.x * right.y - left.y * right.x;

		return result;
	}

	template<typename T>
	const TVec4<T> TVec4<T>::Zero = { 0, 0, 0, 0 };

	template<typename T>
	const TVec4<T> TVec4<T>::One = { 1, 1, 1, 1 };

	template<typename T>
	constexpr TVec4<T>::TVec4(T x, T y, T z, T w)
		: x(x), y(y), z(z), w(w)
	{
	}

	template<typename T>
	constexpr T& TVec4<T>::operator[](size_t idx)
	{
		return elements[idx];
	}

	template<typename T>
	constexpr T TVec4<T>::operator[](size_t idx) const
	{
		return elements[idx];
	}

	template<typename T>
	constexpr TVec4<T> operator-(TVec4<T> a)
	{
		TVec4<T> result;
		result.x = -a.x;
		result.y = -a.y;
		result.z = -a.z;
		result.w = -a.w;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator+(TVec4<T> left, T right)
	{
		TVec4<T> result;
		result.x = left.x + right;
		result.y = left.y + right;
		result.z = left.z + right;
		result.w = left.w + right;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator+=(TVec4<T>& left, T right)
	{
		left.x = left.x + right;
		left.y = left.y + right;
		left.z = left.z + right;
		left.w = left.w + right;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator-(TVec4<T> left, T right)
	{
		TVec4<T> result;
		result.x = left.x - right;
		result.y = left.y - right;
		result.z = left.z - right;
		result.w = left.w - right;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator-=(TVec4<T>& left, T right)
	{
		left.x = left.x - right;
		left.y = left.y - right;
		left.z = left.z - right;
		left.w = left.w - right;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator*(TVec4<T> left, T right)
	{
		TVec4<T> result;
		result.x = left.x * right;
		result.y = left.y * right;
		result.z = left.z * right;
		result.w = left.w * right;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator*=(TVec4<T>& left, T right)
	{
		left.x = left.x * right;
		left.y = left.y * right;
		left.z = left.z * right;
		left.w = left.w * right;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator/(TVec4<T> left, T right)
	{
		TVec4<T> result;
		result.x = left.x / right;
		result.y = left.y / right;
		result.z = left.z / right;
		result.w = left.w / right;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator/=(TVec4<T>& left, T right)
	{
		left.x = left.x / right;
		left.y = left.y / right;
		left.z = left.z / right;
		left.w = left.w / right;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator+(TVec4<T> left, TVec4<T> right)
	{
		TVec4<T> result;
		result.x = left.x + right.x;
		result.y = left.y + right.y;
		result.z = left.z + right.z;
		result.w = left.w + right.w;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator+=(TVec4<T>& left, TVec4<T> right)
	{
		left.x = left.x + right.x;
		left.y = left.y + right.y;
		left.z = left.z + right.z;
		left.w = left.w + right.w;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator-(TVec4<T> left, TVec4<T> right)
	{
		TVec4<T> result;
		result.x = left.x - right.x;
		result.y = left.y - right.y;
		result.z = left.z - right.z;
		result.w = left.w - right.w;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator-=(TVec4<T>& left, TVec4<T> right)
	{
		left.x = left.x - right.x;
		left.y = left.y - right.y;
		left.z = left.z - right.z;
		left.w = left.w - right.w;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator*(TVec4<T> left, TVec4<T> right)
	{
		TVec4<T> result;
		result.x = left.x * right.x;
		result.y = left.y * right.y;
		result.z = left.z * right.z;
		result.w = left.w * right.w;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator*=(TVec4<T>& left, TVec4<T> right)
	{
		left.x = left.x * right.x;
		left.y = left.y * right.y;
		left.z = left.z * right.z;
		left.w = left.w * right.w;

		return left;
	}

	template<typename T>
	constexpr TVec4<T> operator/(TVec4<T> left, TVec4<T> right)
	{
		TVec4<T> result;
		result.x = left.x / right.x;
		result.y = left.y / right.y;
		result.z = left.z / right.z;
		result.w = left.w / right.w;

		return result;
	}

	template<typename T>
	constexpr TVec4<T> operator/=(TVec4<T>& left, TVec4<T> right)
	{
		left.x = left.x / right.x;
		left.y = left.y / right.y;
		left.z = left.z / right.z;
		left.w = left.w / right.w;

		return left;
	}

	template<typename T>
	constexpr bool operator==(TVec4<T> left, TVec4<T> right)
	{
		return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
	}

	template<typename T>
	constexpr T LengthSq(TVec4<T> a)
	{
		return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
	}

	template<typename T>
	constexpr T Length(TVec4<T> a)
	{
		return Sqrt(LengthSq(a));
	}

	template<typename T>
	constexpr TVec4<T> Normalize(TVec4<T> a)
	{
		T lengthSq = LengthSq(a);
		if (lengthSq > 0)
		{
			return a * InvSqrt(lengthSq);
		}

		return TVec4<T>::Zero;
	}

	template<typename T>
	constexpr TVec4<T> Lerp(TVec4<T> a, TVec4<T> b, T t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	constexpr T Dot(TVec4<T> left, TVec4<T> right)
	{
		return left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
	}

	template<typename T>
	const TQuat<T> TQuat<T>::Identity = { 0, 0, 0, 1 };

	template<typename T>
	constexpr TQuat<T>::TQuat(T x, T y, T z, T w)
		: x(x), y(y), z(z), w(w)
	{
	}

	template<typename T>
	constexpr T& TQuat<T>::operator[](size_t idx)
	{
		return elements[idx];
	}

	template<typename T>
	constexpr T TQuat<T>::operator[](size_t idx) const
	{
		return elements[idx];
	}

	template<typename T>
	constexpr TQuat<T> operator*(TQuat<T> left, T right)
	{
		TQuat<T> result;
		result.x = left.x * right;
		result.y = left.y * right;
		result.z = left.z * right;
		result.w = left.w * right;

		return result;
	}

	template<typename T>
	constexpr TQuat<T> operator*=(TQuat<T>& left, T right)
	{
		left.x = left.x * right;
		left.y = left.y * right;
		left.z = left.z * right;
		left.w = left.w * right;

		return left;
	}

	template<typename T>
	constexpr TQuat<T> operator/(TQuat<T> left, T right)
	{
		TQuat<T> result;
		result.x = left.x / right;
		result.y = left.y / right;
		result.z = left.z / right;
		result.w = left.w / right;

		return result;
	}

	template<typename T>
	constexpr TQuat<T> operator/=(TQuat<T>& left, T right)
	{
		left.x = left.x / right;
		left.y = left.y / right;
		left.z = left.z / right;
		left.w = left.w / right;

		return left;
	}

	template<typename T>
	constexpr TQuat<T> operator+(TQuat<T> left, TQuat<T> right)
	{
		TQuat<T> result;
		result.x = left.x + right.x;
		result.y = left.y + right.y;
		result.z = left.z + right.z;
		result.w = left.w + right.w;

		return result;
	}

	template<typename T>
	constexpr TQuat<T> operator+=(TQuat<T>& left, TQuat<T> right)
	{
		left.x = left.x + right.x;
		left.y = left.y + right.y;
		left.z = left.z + right.z;
		left.w = left.w + right.w;

		return left;
	}

	template<typename T>
	constexpr TQuat<T> operator-(TQuat<T> left, TQuat<T> right)
	{
		TQuat<T> result;
		result.x = left.x - right.x;
		result.y = left.y - right.y;
		result.z = left.z - right.z;
		result.w = left.w - right.w;

		return result;
	}

	template<typename T>
	constexpr TQuat<T> operator-=(TQuat<T>& left, TQuat<T> right)
	{
		left.x = left.x - right.x;
		left.y = left.y - right.y;
		left.z = left.z - right.z;
		left.w = left.w - right.w;

		return left;
	}

	template<typename T>
	constexpr TQuat<T> operator*(TQuat<T> left, TQuat<T> right)
	{
		TQuat<T> result;
		result.x = left.x * right.w + left.w * right.x + left.y * right.z - left.z * right.y;
		result.y = left.y * right.w + left.w * right.y + left.z * right.x - left.x * right.z;
		result.z = left.z * right.w + left.w * right.z + left.x * right.y - left.y * right.x;
		result.w = left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z;

		return result;
	}

	template<typename T>
	constexpr TQuat<T> operator*=(TQuat<T>& left, TQuat<T> right)
	{
		return left = left * right;
	}

	template<typename T>
	constexpr T LengthSq(TQuat<T> a)
	{
		return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
	}

	template<typename T>
	constexpr T Length(TQuat<T> a)
	{
		return Sqrt(LengthSq(a));
	}

	template<typename T>
	constexpr TQuat<T> Normalize(TQuat<T> a)
	{
		T lengthSq = LengthSq(a);
		if (lengthSq > 0)
		{
			return a * InvSqrt(lengthSq);
		}

		return TQuat<T>::Identity;
	}

	template<typename T>
	constexpr TQuat<T> Lerp(TQuat<T> a, TQuat<T> b, T t)
	{
		TQuat<T> result = a + (b - a) * t;
		result = Normalize(result);

		return result;
	}

	template<typename T>
	constexpr T Dot(TQuat<T> left, TQuat<T> right)
	{
		return left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
	}

	template<typename T>
	constexpr TQuat<T> Conjugate(TQuat<T> a)
	{
		TQuat<T> result;
		result.x = -a.x;
		result.y = -a.y;
		result.z = -a.z;
		result.w = a.w;

		return result;
	}

	template<typename T>
	constexpr TVec3<T> RotateVector(TQuat<T> rotation, TVec3<T> v)
	{
		TVec3<T> q(rotation.x, rotation.y, rotation.z);
		TVec3<T> t = Cross(q, v) * T(2);

		return v + t * rotation.w + Cross(q, t);
	}

	template<typename T>
	constexpr TQuat<T> AxisAngleRotation(TVec3<T> axis, T angle)
	{
		TVec3<T> axisNorm = Normalize(axis);
		T halfAngle = angle / 2;

		T s = Sin(halfAngle);
		T c = Cos(halfAngle);

		TQuat<T> result;
		result.x = axisNorm.x * s;
		result.y = axisNorm.y * s;
		result.z = axisNorm.z * s;
		result.w = c;

		return result;
	}

	template<typename T>
	const TMat4<T> TMat4<T>::Identity = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};

	template<typename T>
	constexpr TMat4<T>::TMat4(TVec4<T> c0, TVec4<T> c1, TVec4<T> c2, TVec4<T> c3)
		: columns(c0, c1, c2, c3)
	{
	}

	template<typename T>
	constexpr TVec4<T>& TMat4<T>::operator[](size_t idx)
	{
		return columns[idx];
	}

	template<typename T>
	constexpr TVec4<T> TMat4<T>::operator[](size_t idx) const
	{
		return columns[idx];
	}

	template<typename T>
	constexpr TVec4<T> operator*(TMat4<T> left, TVec4<T> right)
	{
		TVec4<T> result;
		result.x = left.columns[0].x * right.x + left.columns[1].x * right.y + left.columns[2].x * right.z + left.columns[3].x * right.w;
		result.y = left.columns[0].y * right.x + left.columns[1].y * right.y + left.columns[2].y * right.z + left.columns[3].y * right.w;
		result.z = left.columns[0].z * right.x + left.columns[1].z * right.y + left.columns[2].z * right.z + left.columns[3].z * right.w;
		result.w = left.columns[0].w * right.x + left.columns[1].w * right.y + left.columns[2].w * right.z + left.columns[3].w * right.w;

		return result;
	}

	template<typename T>
	constexpr TMat4<T> operator*(TMat4<T> left, TMat4<T> right)
	{
		TMat4<T> result;
		result.columns[0] = left * right.columns[0];
		result.columns[1] = left * right.columns[1];
		result.columns[2] = left * right.columns[2];
		result.columns[3] = left * right.columns[3];

		return result;
	}

	template<typename T>
	constexpr TMat4<T> operator*=(TMat4<T>& left, TMat4<T> right)
	{
		return left = left * right;
	}

	template<typename T>
	constexpr TMat4<T> TranslationMatrix(TVec3<T> translation)
	{
		TMat4<T> result = TMat4<T>::Identity;
		result.columns[3].x = translation.x;
		result.columns[3].y = translation.y;
		result.columns[3].z = translation.z;

		return result;
	}

	template<typename T>
	constexpr TMat4<T> RotationMatrix(TQuat<T> rotation)
	{
		TQuat<T> rotationNorm = Normalize(rotation);

		T xx = rotationNorm.x * rotationNorm.x;
		T yy = rotationNorm.y * rotationNorm.y;
		T zz = rotationNorm.z * rotationNorm.z;
		T xy = rotationNorm.x * rotationNorm.y;
		T xz = rotationNorm.x * rotationNorm.z;
		T yz = rotationNorm.y * rotationNorm.z;
		T wx = rotationNorm.w * rotationNorm.x;
		T wy = rotationNorm.w * rotationNorm.y;
		T wz = rotationNorm.w * rotationNorm.z;

		TMat4<T> result;

		result.columns[0].x = 1 - 2 * (yy + zz);
		result.columns[0].y = 2 * (xy + wz);
		result.columns[0].z = 2 * (xz - wy);
		result.columns[0].w = 0;

		result.columns[1].x = 2 * (xy - wz);
		result.columns[1].y = 1 - 2 * (xx + zz);
		result.columns[1].z = 2 * (yz + wx);
		result.columns[1].w = 0;

		result.columns[2].x = 2 * (xz + wy);
		result.columns[2].y = 2 * (yz - wx);
		result.columns[2].z = 1 - 2 * (xx + yy);
		result.columns[2].w = 0;

		result.columns[3].x = 0;
		result.columns[3].y = 0;
		result.columns[3].z = 0;
		result.columns[3].w = 1;

		return result;
	}

	template<typename T>
	constexpr TMat4<T> ScaleMatrix(TVec3<T> scale)
	{
		TMat4<T> result = TMat4<T>::Identity;
		result.columns[0].x = scale.x;
		result.columns[1].y = scale.y;
		result.columns[2].z = scale.z;

		return result;
	}

	template<typename T>
	constexpr TMat4<T> PerspectiveMatrix(T halfFov, T aspectRatio, T near, T far)
	{
		T cotangent = 1 / Tan(halfFov);

		TMat4<T> result = {};
		result.columns[0].x = cotangent / aspectRatio;
		result.columns[1].y = cotangent;
		result.columns[2].w = -1;
		result.columns[2].z = far / (near - far);
		result.columns[3].z = near * far / (near - far);

		return result;
	}
}
