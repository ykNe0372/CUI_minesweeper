#pragma once
#include <array>

template<class T, size_t MAXSIZE> class PoolAllocator
{
public:
	// コンストラクタ
	PoolAllocator() {
		// freelistの初期化
		for (size_t i = 0; i < MAXSIZE - 1; ++i) {
            pool[i].next = &pool[i + 1];  // 次の空き場所を指す
        }

        pool[MAXSIZE - 1].next = nullptr; // 一番後ろの次は nullptr
        freelist = &pool[0]; 			  // 今使える最初のメモリの場所
	}

	// デストラクタ
	~PoolAllocator() {}

	T* Alloc() {
		if (!freelist) return nullptr; // 使い切っている場合は nullptr を返す

		element_type* elem = freelist;
        freelist = freelist->next;	   // 使ったら freelist を次の空き場所に更新
        return reinterpret_cast<T*>(&elem->storage); // メモリ領域を T* 型のポインタに変換して渡す
	}

	void Free(T* addr) {
		if (!addr) return; // 誤動作防止で Free(nullptr) なら何もしない

		element_type* elem = reinterpret_cast<element_type*>(addr); // addr の先頭アドレスを element_type* 型に変換
		elem->next = freelist; // elem の next ポインタを、freelist の先頭に向ける
		freelist = elem; 	   // freelist の先頭を elem に更新
	}

private:
	union element_type {
		// 適切なメモリ配置を持つ、型Tのオブジェクトを格納するのに十分なサイズのメモリ領域の確保
		alignas(T) char storage[sizeof(T)];
		element_type* next;
	};
	std::array<element_type, MAXSIZE> pool;
	element_type* freelist;
};
