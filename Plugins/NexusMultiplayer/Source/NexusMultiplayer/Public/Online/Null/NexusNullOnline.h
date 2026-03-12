#pragma once
#include "Interfaces/INexusOnlineInterface.h"

class NEXUSMULTIPLAYER_API FNexusNullOnline : public INexusOnlineInterface
{
public:
    explicit FNexusNullOnline(UGameInstance* InGameInstance);

    // INexusOnlineInterface
    void CreateSession(const FNexusSessionConfig& Config) override;
    void FindSessions(int32 MaxResults) override;
    void JoinSession(const FOnlineSessionSearchResult& Result) override;
    void JoinSessionById(const FString& SessionId) override;
    void FindSessionByAttribute(const FString& Key, const FString& Value) override;
    void DestroySession() override;

    FString GetDisplayName() const override;
    FString GetUserId()      const override;
    FString GetAuthTicket()  const override;
    bool    IsAvailable()    const override;

    ENexusOnlineMode GetMode() const override
    { return ENexusOnlineMode::Null; }

private:
    TWeakObjectPtr<UGameInstance> GameInstance;
    UWorld* GetWorld() const;

    FDelegateHandle OnCreateDelegateHandle;
    FDelegateHandle OnFindDelegateHandle;
    FDelegateHandle OnJoinDelegateHandle;
    FDelegateHandle OnDestroyDelegateHandle;

    void SetSessionAttribute(const FString& Key, const FString& Value) override;

};
