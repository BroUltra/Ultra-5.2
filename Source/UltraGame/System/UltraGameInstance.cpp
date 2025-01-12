// Copyright Epic Games, Inc. All Rights Reserved.

#include "UltraGameInstance.h"

#include "CommonSessionSubsystem.h"
#include "Components/GameFrameworkComponentManager.h"
#include "HAL/IConsoleManager.h"
#include "UltraGameplayTags.h"
#include "Player/UltraPlayerController.h"
#include "GameFramework/PlayerState.h"

#if UE_WITH_DTLS
#include "DTLSCertStore.h"
#include "DTLSHandlerComponent.h"
#include "Misc/FileHelper.h"
#endif // UE_WITH_DTLS

#include UE_INLINE_GENERATED_CPP_BY_NAME(UltraGameInstance)

namespace Ultra
{
	static bool bTestEncryption = false;
	static FAutoConsoleVariableRef CVarUltraTestEncryption(
		TEXT("Ultra.TestEncryption"),
		bTestEncryption,
		TEXT("If true, clients will send an encryption token with their request to join the server and attempt to encrypt the connection using a debug key. This is NOT SECURE and for demonstration purposes only."),
		ECVF_Default);

#if UE_WITH_DTLS
	static bool bUseDTLSEncryption = false;
	static FAutoConsoleVariableRef CVarUltraUseDTLSEncryption(
		TEXT("Ultra.UseDTLSEncryption"),
		bUseDTLSEncryption,
		TEXT("Set to true if using Ultra.TestEncryption and the DTLS packet handler."),
		ECVF_Default);

	/* Intended for testing with multiple game instances on the same device (desktop builds) */
	static bool bTestDTLSFingerprint = false;
	static FAutoConsoleVariableRef CVarUltraTestDTLSFingerprint(
		TEXT("Ultra.TestDTLSFingerprint"),
		bTestDTLSFingerprint,
		TEXT("If true and using DTLS encryption, generate unique cert per connection and fingerprint will be written to file to simulate passing through an online service."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	static FAutoConsoleCommandWithWorldAndArgs CmdGenerateDTLSCertificate(
		TEXT("GenerateDTLSCertificate"),
		TEXT("Generate a DTLS self-signed certificate for testing and export to PEM."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& InArgs, UWorld* InWorld)
			{
				if (InArgs.Num() == 1)
				{
					const FString& CertName = InArgs[0];

					FTimespan CertExpire = FTimespan::FromDays(365);
					TSharedPtr<FDTLSCertificate> Cert = FDTLSCertStore::Get().CreateCert(CertExpire, CertName);
					if (Cert.IsValid())
					{
						const FString CertPath = FPaths::ProjectContentDir() / TEXT("DTLS") / FPaths::MakeValidFileName(FString::Printf(TEXT("%s.pem"), *CertName));

						if (!Cert->ExportCertificate(CertPath))
						{
							UE_LOG(LogTemp, Error, TEXT("GenerateDTLSCertificate: Failed to export certificate."));
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("GenerateDTLSCertificate: Failed to generate certificate."));
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GenerateDTLSCertificate: Invalid argument(s)."));
				}
			}));
#endif // UE_BUILD_SHIPPING
#endif // UE_WITH_DTLS
};

UUltraGameInstance::UUltraGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUltraGameInstance::Init()
{
	Super::Init();

	// Register our custom init states
	UGameFrameworkComponentManager* ComponentManager = GetSubsystem<UGameFrameworkComponentManager>(this);

	if (ensure(ComponentManager))
	{
		ComponentManager->RegisterInitState(UltraGameplayTags::InitState_Spawned, false, FGameplayTag());
		ComponentManager->RegisterInitState(UltraGameplayTags::InitState_DataAvailable, false, UltraGameplayTags::InitState_Spawned);
		ComponentManager->RegisterInitState(UltraGameplayTags::InitState_DataInitialized, false, UltraGameplayTags::InitState_DataAvailable);
		ComponentManager->RegisterInitState(UltraGameplayTags::InitState_GameplayReady, false, UltraGameplayTags::InitState_DataInitialized);
	}
	
	// Initialize the debug key with a set value for AES256. This is not secure and for example purposes only.
	DebugTestEncryptionKey.SetNum(32);

	for (int32 i = 0; i < DebugTestEncryptionKey.Num(); ++i)
	{
		DebugTestEncryptionKey[i] = uint8(i);
	}

	if (UCommonSessionSubsystem* SessionSubsystem = GetSubsystem<UCommonSessionSubsystem>())
	{
		SessionSubsystem->OnPreClientTravelEvent.AddUObject(this, &UUltraGameInstance::OnPreClientTravelToSession);
	}
}

void UUltraGameInstance::Shutdown()
{
	Super::Shutdown();
}

AUltraPlayerController* UUltraGameInstance::GetPrimaryPlayerController() const
{
	return Cast<AUltraPlayerController>(Super::GetPrimaryPlayerController(false));
}

bool UUltraGameInstance::CanJoinRequestedSession() const
{
	// Temporary first pass:  Always return true
	// This will be fleshed out to check the player's state
	if (!Super::CanJoinRequestedSession())
	{
		return false;
	}
	return true;
}

void UUltraGameInstance::ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate)
{
	// This is a simple implementation to demonstrate using encryption for game traffic using a hardcoded key.
	// For a complete implementation, you would likely want to retrieve the encryption key from a secure source,
	// such as from a web service over HTTPS. This could be done in this function, even asynchronously - just
	// call the response delegate passed in once the key is known. The contents of the EncryptionToken is up to the user,
	// but it will generally contain information used to generate a unique encryption key, such as a user and/or session ID.

	FEncryptionKeyResponse Response(EEncryptionResponse::Failure, TEXT("Unknown encryption failure"));

	if (EncryptionToken.IsEmpty())
	{
		Response.Response = EEncryptionResponse::InvalidToken;
		Response.ErrorMsg = TEXT("Encryption token is empty.");
	}
	else
	{
#if UE_WITH_DTLS
		if (Ultra::bUseDTLSEncryption)
		{
			TSharedPtr<FDTLSCertificate> Cert;

			if (Ultra::bTestDTLSFingerprint)
			{
				// Generate server cert for this identifier, post the fingerprint
				FTimespan CertExpire = FTimespan::FromHours(4);
				Cert = FDTLSCertStore::Get().CreateCert(CertExpire, EncryptionToken);
			}
			else
			{
				// Load cert from disk for testing purposes (never in production)
				const FString CertPath = FPaths::ProjectContentDir() / TEXT("DTLS") / TEXT("UltraTest.pem");

				Cert = FDTLSCertStore::Get().GetCert(EncryptionToken);

				if (!Cert.IsValid())
				{
					Cert = FDTLSCertStore::Get().ImportCert(CertPath, EncryptionToken);
				}
			}

			if (Cert.IsValid())
			{
				if (Ultra::bTestDTLSFingerprint)
				{
					// Fingerprint should be posted to a secure web service for discovery
					// Writing to disk for local testing
					TArrayView<const uint8> Fingerprint = Cert->GetFingerprint();

					FString DebugFile = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("DTLS")) / FPaths::MakeValidFileName(EncryptionToken) + TEXT("_server.txt");

					FString FingerprintStr = BytesToHex(Fingerprint.GetData(), Fingerprint.Num());
					FFileHelper::SaveStringToFile(FingerprintStr, *DebugFile);
				}

				// Server currently only needs the identifier
				Response.EncryptionData.Identifier = EncryptionToken;
				Response.EncryptionData.Key = DebugTestEncryptionKey;

				Response.Response = EEncryptionResponse::Success;
			}
			else
			{
				Response.Response = EEncryptionResponse::Failure;
				Response.ErrorMsg = TEXT("Unable to obtain certificate.");
			}
		}
		else
#endif // UE_WITH_DTLS
		{
			Response.Response = EEncryptionResponse::Success;
			Response.EncryptionData.Key = DebugTestEncryptionKey;
		}
	}

	Delegate.ExecuteIfBound(Response);
}

void UUltraGameInstance::ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate)
{
	// This is a simple implementation to demonstrate using encryption for game traffic using a hardcoded key.
	// For a complete implementation, you would likely want to retrieve the encryption key from a secure source,
	// such as from a web service over HTTPS. This could be done in this function, even asynchronously - just
	// call the response delegate passed in once the key is known.

	FEncryptionKeyResponse Response;

#if UE_WITH_DTLS
	if (Ultra::bUseDTLSEncryption)
	{
		Response.Response = EEncryptionResponse::Failure;

		APlayerController* const PlayerController = GetFirstLocalPlayerController();

		if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->GetUniqueId().IsValid())
		{
			const FUniqueNetIdRepl& PlayerUniqueId = PlayerController->PlayerState->GetUniqueId();

			// Ideally the encryption token is passed in directly rather than having to attempt to rebuild it
			const FString EncryptionToken = PlayerUniqueId.ToString();

			Response.EncryptionData.Identifier = EncryptionToken;

			// Server's fingerprint should be pulled from a secure service
			if (Ultra::bTestDTLSFingerprint)
			{
				// But for testing purposes...
				FString DebugFile = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("DTLS")) / FPaths::MakeValidFileName(EncryptionToken) + TEXT("_server.txt");
				FString FingerprintStr;
				FFileHelper::LoadFileToString(FingerprintStr, *DebugFile);

				Response.EncryptionData.Fingerprint.AddUninitialized(FingerprintStr.Len() / 2);
				HexToBytes(FingerprintStr, Response.EncryptionData.Fingerprint.GetData());
			}
			else
			{
				// Pulling expected fingerprint from disk for testing, this should come from a secure service
				const FString CertPath = FPaths::ProjectContentDir() / TEXT("DTLS") / TEXT("UltraTest.pem");

				TSharedPtr<FDTLSCertificate> Cert = FDTLSCertStore::Get().GetCert(EncryptionToken);
				if (!Cert.IsValid())
				{
					Cert = FDTLSCertStore::Get().ImportCert(CertPath, EncryptionToken);
				}

				if (Cert.IsValid())
				{
					TArrayView<const uint8> Fingerprint = Cert->GetFingerprint();

					Response.EncryptionData.Fingerprint = Fingerprint;
				}
				else
				{
					Response.Response = EEncryptionResponse::Failure;
					Response.ErrorMsg = TEXT("Unable to obtain certificate.");
				}
			}

			Response.EncryptionData.Key = DebugTestEncryptionKey;

			Response.Response = EEncryptionResponse::Success;
		}
	}
	else
#endif // UE_WITH_DTLS
	{
		Response.Response = EEncryptionResponse::Success;
		Response.EncryptionData.Key = DebugTestEncryptionKey;
	}

	Delegate.ExecuteIfBound(Response);
}

void UUltraGameInstance::OnPreClientTravelToSession(FString& URL)
{
	// Add debug encryption token if desired.
	if (Ultra::bTestEncryption)
	{
#if UE_WITH_DTLS
		if (Ultra::bUseDTLSEncryption)
		{
			APlayerController* const PlayerController = GetFirstLocalPlayerController();

			if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->GetUniqueId().IsValid())
			{
				const FUniqueNetIdRepl& PlayerUniqueId = PlayerController->PlayerState->GetUniqueId();
				const FString EncryptionToken = PlayerUniqueId.ToString();

				URL += TEXT("?EncryptionToken=") + EncryptionToken;
			}
		}
		else
#endif // UE_WITH_DTLS
		{
			// This is just a value for testing/debugging, the server will use the same key regardless of the token value.
			// But the token could be a user ID and/or session ID that would be used to generate a unique key per user and/or session, if desired.
			URL += TEXT("?EncryptionToken=1");
		}
	}
}