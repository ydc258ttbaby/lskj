#pragma once
#include <functional>


#ifdef FLOWCAM_EXPORTS
#define FLOWCAM_EXPORTS __declspec(dllexport)
#else
#define FLOWCAM_EXPORTS __declspec(dllimport)
#endif

class FLOWCAM_EXPORTS IInterface
{
protected:
	IInterface();

	virtual ~IInterface();

public:
	static IInterface* CreateInstance();

	static void DestoryInstance(IInterface* pIShowUI);

	//��ʼ��
	virtual void Init() = 0;

	//����״̬�ȵĻص�����
	//virtual void SetInitStateCallback(function<void(int, int, int, int)>* pCallback) = 0;
	virtual void SetInitStateCallback(void(*pCallback)(int, int, int, int)) = 0;

	//��ʼ����
	virtual bool CollectStart(long i_Volume, long i_FlowRate, long i_PlateType,
		long i_RowIndex, long i_ColumnIndex) = 0;

	/// <summary>
	/// ֹͣ����
	/// </summary>
	/// <returns></returns>
	virtual bool StopCollect() = 0;

	/// <summary>
	/// ά��
	/// </summary>
	/// <returns></returns>
	virtual bool Maintain(int functionType) = 0;

	/// <summary>
	/// �Լ�
	/// </summary>
	/// <returns></returns>
	virtual bool SelfCheck(int functionType) = 0;

	/// <summary>
	/// �ر�����
	/// </summary>
	/// <returns></returns>
	virtual bool ShutDown() = 0;

	/// <summary>
	/// ���ز���
	/// </summary>
	/// <returns></returns>
	virtual bool OpenCloseDoor() = 0;
};

