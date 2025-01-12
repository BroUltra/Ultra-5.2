// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PawnComponent.h"
#include "Cosmetics/UltraCosmeticAnimationTypes.h"
#include "UltraCharacterPartTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "UltraPawnComponent_CharacterParts.generated.h"

class UUltraPawnComponent_CharacterParts;
namespace EEndPlayReason { enum Type : int; }
struct FGameplayTag;
struct FUltraCharacterPartList;

class AActor;
class UChildActorComponent;
class UObject;
class USceneComponent;
class USkeletalMeshComponent;
struct FFrame;
struct FNetDeltaSerializeInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUltraSpawnedCharacterPartsChanged, UUltraPawnComponent_CharacterParts*, ComponentWithChangedParts);

//////////////////////////////////////////////////////////////////////

// A single applied character part
USTRUCT()
struct FUltraAppliedCharacterPartEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FUltraAppliedCharacterPartEntry()
	{}

	FString GetDebugString() const;

private:
	friend FUltraCharacterPartList;
	friend UUltraPawnComponent_CharacterParts;

private:
	// The character part being represented
	UPROPERTY()
	FUltraCharacterPart Part;

	// Handle index we returned to the user (server only)
	UPROPERTY(NotReplicated)
	int32 PartHandle = INDEX_NONE;

	// The spawned actor instance (client only)
	UPROPERTY(NotReplicated)
	TObjectPtr<UChildActorComponent> SpawnedComponent = nullptr;
};

//////////////////////////////////////////////////////////////////////

// Replicated list of applied character parts
USTRUCT(BlueprintType)
struct FUltraCharacterPartList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUltraCharacterPartList()
		: OwnerComponent(nullptr)
	{
	}

	FUltraCharacterPartList(UUltraPawnComponent_CharacterParts* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUltraAppliedCharacterPartEntry, FUltraCharacterPartList>(Entries, DeltaParms, *this);
	}

	FUltraCharacterPartHandle AddEntry(FUltraCharacterPart NewPart);
	void RemoveEntry(FUltraCharacterPartHandle Handle);
	void ClearAllEntries(bool bBroadcastChangeDelegate);

	FGameplayTagContainer CollectCombinedTags() const;
private:
	friend UUltraPawnComponent_CharacterParts;

	bool SpawnActorForEntry(FUltraAppliedCharacterPartEntry& Entry);
	bool DestroyActorForEntry(FUltraAppliedCharacterPartEntry& Entry);

private:
	// Replicated list of equipment entries
	UPROPERTY()
	TArray<FUltraAppliedCharacterPartEntry> Entries;

	// The component that contains this list
	UPROPERTY(NotReplicated)
	TObjectPtr<UUltraPawnComponent_CharacterParts> OwnerComponent;

	// Upcounter for handles
	int32 PartHandleCounter = 0;
};

template<>
struct TStructOpsTypeTraits<FUltraCharacterPartList> : public TStructOpsTypeTraitsBase2<FUltraCharacterPartList>
{
	enum { WithNetDeltaSerializer = true };
};

//////////////////////////////////////////////////////////////////////

// A component that handles spawning cosmetic actors attached to the owner pawn on all clients
UCLASS(meta=(BlueprintSpawnableComponent))
class UUltraPawnComponent_CharacterParts : public UPawnComponent
{
	GENERATED_BODY()

public:
	UUltraPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	// Adds a character part to the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	FUltraCharacterPartHandle AddCharacterPart(const FUltraCharacterPart& NewPart);

	// Removes a previously added character part from the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveCharacterPart(FUltraCharacterPartHandle Handle);

	// Removes all added character parts, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveAllCharacterParts();

	// Gets the list of all spawned character parts from this component
	UFUNCTION(BlueprintCallable, BlueprintPure=false, BlueprintCosmetic, Category=Cosmetics)
	TArray<AActor*> GetCharacterPartActors() const;

	// If the parent actor is derived from ACharacter, returns the Mesh component, otherwise nullptr
	USkeletalMeshComponent* GetParentMeshComponent() const;

	// Returns the scene component to attach the spawned actors to
	// If the parent actor is derived from ACharacter, we'll use the Mesh component, otherwise the root component
	USceneComponent* GetSceneComponentToAttachTo() const;

	// Returns the set of combined gameplay tags from attached character parts, optionally filtered to only tags that start with the specified root
	UFUNCTION(BlueprintCallable, BlueprintPure=false, BlueprintCosmetic, Category=Cosmetics)
	FGameplayTagContainer GetCombinedTags(FGameplayTag RequiredPrefix) const;

	void BroadcastChanged();

public:
	// Delegate that will be called when the list of spawned character parts has changed
	UPROPERTY(BlueprintAssignable, Category=Cosmetics, BlueprintCallable)
	FUltraSpawnedCharacterPartsChanged OnCharacterPartsChanged;

private:
	// List of character parts
	UPROPERTY(Replicated)
	FUltraCharacterPartList CharacterPartList;

	// Rules for how to pick a body style mesh for animation to play on, based on character part cosmetics tags
	UPROPERTY(EditAnywhere, Category=Cosmetics)
	FUltraAnimBodyStyleSelectionSet BodyMeshes;
};
