#pragma once
#include <cstddef>
#include <cstdint>

namespace Settings {

    struct Overdrive {
        bool sixthGear    = true;
        bool includeStock = false;
        bool tranny       = true;
    };

    bool Load(void* module, Overdrive& overdrive);

}

namespace Attrib {

    using Key = unsigned int;

    class Array {
      public:
        float* Data() {
            return reinterpret_cast<float*>(this + 1);
        }

        uint16_t mAlloc;
        uint16_t mCount;
        uint16_t mSize;
        uint16_t mEncodedTypePad;
    };

    class RefSpec {
      public:
        Key               mClassKey;
        Key               mCollectionKey;
        const void*       mCollectionPtr;
    };

    class Collection {
      public:
        const void* GetData(Key attributeKey, unsigned int index) const;

        unsigned char     mTable[0x10];
        const Collection* mParent;
        void*             mClass;
        void*             mLayout;
        unsigned int      mRefCount;
        Key               mKey;
        void*             mSource;
    };

    const Collection* FindCollection(Key classKey, Key collectionKey);

    class Class {
      public:
        Key               GetFirstCollection() const;
        Key               GetNextCollection(Key prev) const;
        const Collection* GetCollection(Key collectionKey) const;
    };

    class Database {
      public:
        static Database& Get();

        Class* GetClass(Key classKey) const;
    };

    class Instance {
      public:
        void*             mOwner;
        const Collection* mCollection;
        void*             mLayoutPtr;
        uint32_t          mMsgPort;
        uint16_t          mFlags;
        uint16_t          mLocks;
    };

    namespace Gen {

        class pvehicle : public Instance {};

        class engineaudio : public Instance {
          public:
            struct _LayoutStruct {
                unsigned char mBeforeCarID[0x94];
                uint32_t      CarID;
                unsigned char mBeforeTranny[0x0B];
                bool          Tranny;
            };

            _LayoutStruct* GetLayout() const {
                return static_cast<_LayoutStruct*>(mLayoutPtr);
            }
        };

        class transmission : public Instance {
          public:
            struct _LayoutStruct {
                Array GEAR_RATIO;
            };
        };

    }

}

template <typename T>
class AttributeStructPtr : public T {
  public:
    AttributeStructPtr* ConstructHook(unsigned int key);
    AttributeStructPtr* Construct(unsigned int key);
};

namespace Physics {
namespace Upgrades {

    enum Type {
        PUT_TIRES        = 0,
        PUT_BRAKES       = 1,
        PUT_CHASSIS      = 2,
        PUT_TRANSMISSION = 3,
        PUT_ENGINE       = 4,
        PUT_INDUCTION    = 5,
        PUT_NOS          = 6,
        PUT_MAX          = 7,
    };

    int GetLevel(const Attrib::Gen::pvehicle& vehicle, Type type);
    int GetMaxLevel(const Attrib::Gen::pvehicle& vehicle, Type type);

}
}

namespace Sound {

    enum Context {
        CONTEXT_PLAYER = 0,
    };

}

class EAX_CarState {
  public:
    unsigned char            mBeforeAttributes[0x200];
    Attrib::Gen::pvehicle    mAttributes;
    Attrib::Gen::engineaudio mEngineInfo;
    Sound::Context           mContext;
};

class CSTATE_Base {
  public:
    EAX_CarState* GetPhysCar() const {
        return m_pCar;
    }

    unsigned char mBeforeCar[0x34];
    EAX_CarState* m_pCar;
};

class EAXCar {
  public:
    Attrib::Gen::engineaudio& GetAttributes() {
        return mEngineInfo;
    }

    unsigned char            mBeforeEngineInfo[0xD0];
    Attrib::Gen::engineaudio mEngineInfo;
};

class CARSFX_GinsuEngine {
  public:
    void InitializeEngineHook();
    void InitializeEngine();

    EAX_CarState* GetPhysCar() const {
        return m_pStateBase->GetPhysCar();
    }

    void*         mVTable;
    void*         m_pNextSFX;
    bool          m_bIsEnabled;
    int*          m_pOutPutBlock;
    int*          m_pInputBlock;
    CSTATE_Base*  m_pStateBase;
    EAXCar*       m_pEAXCar;
};

namespace Overdrive {

    bool Init(void* module);
    void Restore();

}
