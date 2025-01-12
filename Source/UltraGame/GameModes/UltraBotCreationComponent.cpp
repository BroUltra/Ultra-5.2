// Copyright Epic Games, Inc. All Rights Reserved.

#include "UltraBotCreationComponent.h"
#include "UltraGameMode.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/UltraExperienceManagerComponent.h"
#include "Development/UltraDeveloperSettings.h"
#include "Character/UltraPawnExtensionComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Character/UltraDespawnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UltraBotCreationComponent)

UUltraBotCreationComponent::UUltraBotCreationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUltraBotCreationComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for the experience load to complete
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UUltraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UUltraExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_LowPriority(FOnUltraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UUltraBotCreationComponent::OnExperienceLoaded(const UUltraExperienceDefinition* Experience)
{
#if WITH_SERVER_CODE
	if (HasAuthority())
	{
		ServerCreateBots();
	}
#endif
}

#if WITH_SERVER_CODE

void UUltraBotCreationComponent::ServerCreateBots()
{
	if (BotControllerClass == nullptr)
	{
		return;
	}

	RemainingBotNames = RandomBotNames;

	// Determine how many bots to spawn
	int32 EffectiveBotCount = NumBotsToCreate;

	// Give the developer settings a chance to override it
	if (GIsEditor)
	{
		const UUltraDeveloperSettings* DeveloperSettings = GetDefault<UUltraDeveloperSettings>();
		
		if (DeveloperSettings->bOverrideBotCount)
		{
			EffectiveBotCount = DeveloperSettings->OverrideNumPlayerBotsToSpawn;
		}
	}

	// Give the URL a chance to override it
	if (AGameModeBase* GameModeBase = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameModeBase->OptionsString, TEXT("NumBots"), EffectiveBotCount);
	}

	// Create them
	for (int32 Count = 0; Count < EffectiveBotCount; ++Count)
	{
		SpawnOneBot();
	}
}

FString UUltraBotCreationComponent::CreateBotName(int32 PlayerIndex)
{
	FString Result;
	if (RemainingBotNames.Num() > 0)
	{
		const int32 NameIndex = FMath::RandRange(0, RemainingBotNames.Num() - 1);
		Result = RemainingBotNames[NameIndex];
		RemainingBotNames.RemoveAtSwap(NameIndex);
	}
	else
	{
		//@TODO: PlayerId is only being initialized for players right now
		PlayerIndex = FMath::RandRange(260, 260+100);
		Result = FString::Printf(TEXT("Tinplate %d"), PlayerIndex);
	}
	return Result;
}

void UUltraBotCreationComponent::SpawnOneBot()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.OverrideLevel = GetComponentLevel();
	SpawnInfo.ObjectFlags |= RF_Transient;
	AAIController* NewController = GetWorld()->SpawnActor<AAIController>(BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	if (NewController != nullptr)
	{
		AUltraGameMode* GameMode = GetGameMode<AUltraGameMode>();
		check(GameMode);

		if (NewController->PlayerState != nullptr)
		{
			NewController->PlayerState->SetPlayerName(CreateBotName(NewController->PlayerState->GetPlayerId()));
		}

		GameMode->DispatchPostLogin(NewController);
		GameMode->RestartPlayer(NewController);

		if (NewController->GetPawn() != nullptr)
		{
			if (UUltraPawnExtensionComponent* PawnExtComponent = NewController->GetPawn()->FindComponentByClass<UUltraPawnExtensionComponent>())
			{
				PawnExtComponent->CheckDefaultInitialization();
			}
		}

		SpawnedBotList.Add(NewController);
	}
}

void UUltraBotCreationComponent::RemoveOneBot()
{
	if (SpawnedBotList.Num() > 0)
	{
		// Right now this removes a random bot as they're all the same; could prefer to remove one
		// that's high skill or low skill or etc... depending on why you are removing one
		const int32 BotToRemoveIndex = FMath::RandRange(0, SpawnedBotList.Num() - 1);

		AAIController* BotToRemove = SpawnedBotList[BotToRemoveIndex];
		SpawnedBotList.RemoveAtSwap(BotToRemoveIndex);

		if (BotToRemove)
		{
			// If we can find an actor, destroy it
			if (APawn* ControlledPawn = BotToRemove->GetPawn())
			{
				ControlledPawn->Destroy();
			}

			// Destroy the controller (will cause it to Logout, etc...)
			BotToRemove->Destroy();
		}
	}
}

#endif
