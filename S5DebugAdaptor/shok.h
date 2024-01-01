#pragma once
#include <type_traits>

// some minimal shok stuff to interface with
// for more, see https://github.com/mcb5637/S5BinkHook

template<class T>
requires std::is_enum_v<T>
class enum_is_flags : public std::false_type {};

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T& operator&=(T& a, T b) {
	using u = std::underlying_type<T>::type;
	a = static_cast<T>(static_cast<u>(a) & static_cast<u>(b));
	return a;
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T& operator|=(T& a, T b) {
	using u = std::underlying_type<T>::type;
	a = static_cast<T>(static_cast<u>(a) | static_cast<u>(b));
	return a;
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T& operator^=(T& a, T b) {
	using u = std::underlying_type<T>::type;
	a = static_cast<T>(static_cast<u>(a) ^ static_cast<u>(b));
	return a;
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T operator~(T a) {
	using u = std::underlying_type<T>::type;
	return static_cast<T>(~static_cast<u>(a));
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T operator&(T a, T b) {
	a &= b;
	return a;
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T operator|(T a, T b) {
	a |= b;
	return a;
}

template<class T>
	requires std::is_enum_v<T>&& enum_is_flags<T>::value
constexpr T operator^(T a, T b) {
	a ^= b;
	return a;
}


namespace BB {
	class CException {
	public:
		virtual ~CException() = default;
		virtual bool __stdcall CopyMessage(char* buffer, size_t buffLen) const = 0;

		static inline constexpr int vtp = 0x761AA8;
	};

	class IStream {
	public:
		enum class Flags : int {
			None = 0,

			// you need one of these
			CreateAlways = 1, // overrides previous file
			CreateNew = 2,
			OpenExisting = 3,
			OpenAlways = 4,

			// and one of these
			GenericRead = 0x10,
			GenericWrite = 0x20,

			// and one of these
			ShareRead = 0x100,
			ShareWrite = 0x200,

			// optional
			WriteThrough = 0x1000,
			RandomAccess = 0x2000,
			SequentialScan = 0x4000,

			// premade
			DefaultRead = 0x113,
			DefaultWrite = 0x121,
		};
		enum class SeekMode : int {
			Begin = 0,
			Current = 1,
			End = 2,
		};


		virtual ~IStream() = default;
	private:
		virtual bool __stdcall rettrue();
		virtual bool __stdcall rettrue1();
		virtual bool __stdcall rettrue2();
	public:
		virtual const char* __stdcall GetFileName();
		virtual int64_t __stdcall GetLastWriteTime(); // 5 returns in eax and edx, but that should be fine, 0 on memorystream
		virtual size_t __stdcall GetSize();
		virtual void __stdcall SetFileSize(long size); // moves file pointer to eof
		virtual long __stdcall GetFilePointer();
		virtual void __stdcall SetFilePointer(long fp);
		virtual long __stdcall Read(void* buff, long numBytesToRead); // 10 returns num bytes read
		virtual void __stdcall Seek(long seek, SeekMode mode);
		virtual void __stdcall Write(const void* buff, long numBytesToWrite);
	};
	template<>
	class ::enum_is_flags<IStream::Flags> : public std::true_type {};

	class CFileStreamEx : public IStream {
		IStream* RealStream = nullptr;

	public:
		static constexpr int vtp = 0x761C60;

		CFileStreamEx();
		bool OpenFile(const char* filename, Flags mode);
		void Close();
	};
}
