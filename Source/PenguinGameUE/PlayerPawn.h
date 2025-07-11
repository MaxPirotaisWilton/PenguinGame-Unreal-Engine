// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include <GameFramework/SpringArmComponent.h>
#include <Camera/CameraComponent.h>

#include <Paper2D/Classes/PaperFlipbook.h>

#include "PaperSpriteComponent.h"
#include "PaperFlipbookComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "PlayerPawn.generated.h"

enum PenguinState {
	FALLING,
	ONFEET,
	ONBELLY_L,
	ONBELLY_R,
	ONHEAD,
	SWIMMING
};

enum ButtonPressedState {
	RELEASED,
	ENTERED,
	HELD
};

UCLASS()
class PENGUINGAMEUE_API APlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	APlayerPawn();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* overlappedComp, AActor* otherActor, UPrimitiveComponent* otherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* overlappedComp, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

	UFUNCTION()
	void OnHorizontal(float val);

	void HorizontalMovement(float val);

	UFUNCTION()
	void OnJumpButtonEnter();
	UFUNCTION()
	void OnJumpButtonRelease();

	void JumpMovement();

	UFUNCTION()
	void OnFlipbookAnimEnd();


	bool SimplifiedSphereCast(FVector location, float radius);
	void UpdateFlipbookOrientation(bool flipSprite);
	bool IsFlipbookFlipped();
	void DetectCollisionContinuous();
	void SetFlipbookAnimation(UPaperFlipbook* flipbook, bool looping, bool interrupts);

	float CalcAlpha();
	FVector CalcHydroLift(float theta);

	// The player collison box component.
	UPROPERTY(VisibleDefaultsOnly)
	UBoxComponent* boxComponent;

	// The player flipbook component.
	UPROPERTY(VisibleDefaultsOnly)
	UPaperFlipbookComponent* playerFlipbookComp;

	// Array of flipbook animations
	UPaperFlipbook* playerFlipBooks[10];

	// Camera Spring arm
	UPROPERTY(VisibleDefaultsOnly)
	USpringArmComponent* springArm;

	// Camera
	UPROPERTY(VisibleDefaultsOnly)
	UCameraComponent* camera;

	// Particle System & associated component
	UPROPERTY(VisibleDefaultsOnly)
	UNiagaraComponent* niagaraComp;

	UNiagaraSystem* niagaraSystem;

	const float waddleSpeed = 150.0f;
	const float slideFastSpeed = 400.0f;
	const float slidePushMaxSpeed = 550.0f;
	const float swimSpeed = 2400.0f;

	const float jumpStrength = 4500.0f;
	const float pushStrength = 4000.0f;
	const float swimStrength = 10000.0f;

	const float gettingUpStrength = 15000.0f;
	const float reorientJumpStrength = 65000.0f;

	const float bubbleSpawnRate = 250.0f;
	const float swimFlipAngle = 30.0f;

	FVector boxExtents = FVector(42.0f, 1.0f, 58.0f);

	FVector onFeetPos = FVector(0.0f);
	FVector onBRightPos = FVector(0.0f);
	FVector onBLeftPos = FVector(0.0f);
	FVector onHeadPos = FVector(0.0f);

	bool onFeet, onHead, onBellyL, onBellyR, inWater;

	ButtonPressedState jumpButtonState;
	float horizontalAxis;

	bool hasJumped;

	bool nonLoopingAnimPlaying;

	PenguinState playerState;

	TArray<TEnumAsByte<EObjectTypeQuery>> st_ObjectTypes;
	TArray<AActor*> st_ActorsToIgnore;

};
