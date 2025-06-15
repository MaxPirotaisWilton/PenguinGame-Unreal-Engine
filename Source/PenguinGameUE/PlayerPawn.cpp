// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerPawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Particles/ParticleEmitter.h>
#include <cmath>
#include <DrawDebugHelpers.h>

// Constructor
APlayerPawn::APlayerPawn()
{
	//Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Set Physics Material
	physicalMat = LoadObject<UPhysicalMaterial>(nullptr, TEXT("PhysicalMaterial'/Game/Assets/PhysMats/Penguin_PhysMat.Penguin_PhysMat'"));

	//Set the Box Collision component
	boxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollider"));

	boxComponent->SetSimulatePhysics(true);
	boxComponent->SetRelativeScale3D(FVector(1.0f));
	boxComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	boxComponent->SetConstraintMode(EDOFMode::XZPlane);
	boxComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	boxComponent->SetNotifyRigidBodyCollision(true);

	boxComponent->SetBoxExtent(boxExtents, true);

	if (physicalMat) boxComponent->SetPhysMaterialOverride(physicalMat);

	this->SetRootComponent(boxComponent);

	// Setup the spring arm that the camera will attach to.
	springArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	springArm->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	springArm->SetRelativeRotation(FVector(0.0f, -1.0f, 0.0f).Rotation());
	springArm->SetAbsolute(false, true, false);
	springArm->TargetArmLength = 1500.f;
	springArm->bEnableCameraLag = true;
	springArm->CameraLagSpeed = 6.0f;

	// Setup the camera.
	camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	camera->AttachToComponent(springArm, FAttachmentTransformRules::KeepWorldTransform, USpringArmComponent::SocketName);
	camera->SetWorldRotation(FVector(0.0f, -1.0f, 0.0f).Rotation());
	camera->ProjectionMode = ECameraProjectionMode::Perspective;
	camera->FieldOfView = 70;

	// Setup Flipbook array

	playerFlipBooks[0] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_Falling.Penguin_Falling'"));
	playerFlipBooks[1] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_Idle.Penguin_Idle'"));
	playerFlipBooks[2] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_Waddle.Penguin_Waddle'"));
	playerFlipBooks[3] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_Belly.Penguin_Belly'"));
	playerFlipBooks[4] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_BellyFast.Penguin_BellyFast'"));
	playerFlipBooks[5] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_SlidePush.Penguin_SlidePush'"));
	playerFlipBooks[6] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_OnHead.Penguin_OnHead'"));
	playerFlipBooks[7] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_InWater.Penguin_InWater'"));
	playerFlipBooks[8] = LoadObject<UPaperFlipbook>(nullptr, TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Penguin_Swimming.Penguin_Swimming'"));

	// Create the animated sprite component.
	playerFlipbookComp = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("PlayerVisual"));
	playerFlipbookComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	playerFlipbookComp->SetFlipbook(playerFlipBooks[1]);
	playerFlipbookComp->SetRelativeLocation(FVector(0.0f));
	playerFlipbookComp->SetRelativeScale3D(FVector(1.0f));

	niagaraSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("NiagaraSystem'/Game/Assets/ParticleSystem/ns_PlayerBubbles.ns_PlayerBubbles'"));

	niagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraParticleSystem"));
	niagaraComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	niagaraComp->SetAsset(niagaraSystem, true);
	niagaraComp->SetWorldLocation(FVector(0.0f, -1.0f, 0.0f));
	niagaraComp->Deactivate();
	niagaraComp->SetAutoActivate(false);
}

// Called when the game starts or when spawned
void APlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	boxComponent->OnComponentBeginOverlap.AddDynamic(this, &APlayerPawn::OnOverlapBegin);
	boxComponent->OnComponentEndOverlap.AddDynamic(this, &APlayerPawn::OnOverlapEnd);

	playerFlipbookComp->OnFinishedPlaying.AddDynamic(this, &APlayerPawn::OnFlipbookAnimEnd);

	// Setup all other variables for Sphere Tracing
	st_ObjectTypes = { UEngineTypes::ConvertToObjectType(ECC_WorldStatic) };
}


// Called every frame
void APlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Detect which side is colliding with the player with Sphere Casts and 
	// updates their state. Otherwise player is deemed to be FALLING.
	playerState = FALLING;
	DetectCollisionContinuous();

	// If player is in Water, set State to SWIMMING
	playerState = inWater ? SWIMMING : playerState;

	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, FString::Printf(TEXT("Playerstate: %d"), playerState));

	float mass = boxComponent->GetMass();

	// Rotate player when falling down if they're still holding the jump button
	if (playerState == FALLING && hasJumped && GetVelocity().Z < 0) {
		boxComponent->AddTorque(FVector(0.0f, 3000.0f * mass * 10.0f * playerFlipbookComp->GetRelativeScale3D().X, 0.0f));
	}

	// Set Swimming physical behaviour and Movement
	if (playerState == SWIMMING)
	{
		// Parameters
		boxComponent->SetEnableGravity(false);
		boxComponent->SetLinearDamping(0.65f);
		boxComponent->SetAngularDamping(0.65f);

		// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, FString::Printf(TEXT("Calculating speed over max speed: %f"), (GetVelocity().SizeSquared() / (swimSpeed * swimSpeed))));

		float normalizedSpeed = GetVelocity().SizeSquared() / (swimSpeed * swimSpeed);

		if (!isnan(normalizedSpeed)) niagaraComp->SetVariableFloat("User.SpawnRate", 190.0f * normalizedSpeed);

		float alpha = CalcAlpha();

		FVector hydroDrag = CalcHydroLift(alpha);

		// boxComponent->AddForce(hydroDrag * 30);


		// SWIM FORWARDS
		if (isPressingJump) {

			SetFlipbookAnimation(playerFlipBooks[8], true, true);
			if (GetVelocity().SizeSquared() < swimSpeed * swimSpeed)
			{
				boxComponent->AddForce(GetActorUpVector() * swimStrength);
			}
		}
		else {
			SetFlipbookAnimation(playerFlipBooks[7], true, true);
		}
	}
	// Reset
	else
	{
		boxComponent->SetEnableGravity(true);
		boxComponent->SetLinearDamping(0.01f);
		boxComponent->SetAngularDamping(0.0f);
	}



}

// Called to bind functionality to input
void APlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APlayerPawn::OnJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APlayerPawn::OnJumpRelease);

	PlayerInputComponent->BindAxis("Horizontal", this, &APlayerPawn::OnHorizontal);
}

void APlayerPawn::OnHorizontal(float val)
{
	if (playerState == FALLING)
	{
		SetFlipbookAnimation(playerFlipBooks[0], true, true);
	}

	// If inputs are registered
	if (GEngine && (val > 0.1f || val < -0.1f))
	{
		float mass = boxComponent->GetMass();

		// WALKING
		if (playerState == ONFEET)
		{
			// Only apply force if the player is slower than maximum defined speed
			// Compares using Squared vector length
			if (GetVelocity().SizeSquared() < waddleSpeed * waddleSpeed)
			{
				boxComponent->AddForce(FVector(1500.0f * val * mass, 0.0f, 0.0f));
			}

			SetFlipbookAnimation(playerFlipBooks[2], true, false);
			UpdateFlipbookOrientation(val < -0.1f);
		}
		// GETTING UP FROM BELLY
		else if (playerState == ONBELLY_L && val > 0.1f)
		{
			// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, TEXT("Attempting standing up from belly Left"));
			boxComponent->AddTorque(FVector(0.0f, 13000.0f * mass * 10.0f, 0.0f));
		}
		else if (playerState == ONBELLY_R && val < -0.1f)
		{
			// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, TEXT("Attempting standing up from belly Left"));
			boxComponent->AddTorque(FVector(0.0f, -13000.0f * mass * 10.0f, 0.0f));
		}
		// STEER SWIMMING
		else if (playerState == SWIMMING) {
			boxComponent->AddTorque(FVector(0.0f, 1400.0f * val * mass * 10.0f, 0.0f));
		}

	}
	else {
		if (playerState == ONFEET)
		{
			// Idle Anim
			SetFlipbookAnimation(playerFlipBooks[1], true, false);
		}
		else if ((playerState == ONBELLY_L || playerState == ONBELLY_R))
		{
			if (GetVelocity().SizeSquared() > slideFastSpeed * slideFastSpeed)
			{
				// Sliding Fast Anim
				SetFlipbookAnimation(playerFlipBooks[4], true, false);
			}
			else
			{
				// Belly Idle Anim
				SetFlipbookAnimation(playerFlipBooks[3], true, false);
			}
			UpdateFlipbookOrientation(playerState == ONBELLY_L);
		}
		else if (playerState == ONHEAD)
		{
			// On Head Idle Anim
			SetFlipbookAnimation(playerFlipBooks[6], true, false);
		}
	}
}

// Called on Frame of input pressed
void APlayerPawn::OnJump()
{
	float mass = boxComponent->GetMass();

	isPressingJump = true;

	// JUMP
	if (playerState == ONFEET)
	{
		boxComponent->AddForce(GetActorUpVector() * jumpStrength * mass * 10.0f);
		hasJumped = true;
	}

	// PUSH SLIDE
	if ((playerState == ONBELLY_L || playerState == ONBELLY_R) && GetVelocity().SizeSquared() < slidePushMaxSpeed * slidePushMaxSpeed)
	{
		boxComponent->AddForce(GetActorUpVector() * pushStrength * mass * 10.0f);

		// Push Slide Anim
		// Note the function is called twice:
		// - First to make sure the Push Slide anim plays from the start
		//	 (Anims only start from beginning if they are new)
		// - Next plays the actual Anim
		SetFlipbookAnimation(playerFlipBooks[4], true, true);
		SetFlipbookAnimation(playerFlipBooks[5], false, true);
	}

	// JUMP FROM ON HEAD
	if (playerState == ONHEAD)
	{
		boxComponent->AddForce(-GetActorUpVector() * jumpStrength * mass * 10.0f);
		boxComponent->AddTorque(FVector(0.0f, 72500.0f * mass * 10.0f * playerFlipbookComp->GetRelativeScale3D().X, 0.0f));
	}
}

// Called on Frame of input released
void APlayerPawn::OnJumpRelease()
{
	isPressingJump = false;
	hasJumped = false;
}

// For simplicity, all overlaps are considered to be touching water

void APlayerPawn::OnOverlapBegin(UPrimitiveComponent* overlappedComp, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, TEXT("OnOverlapBegin Method Called"));

	niagaraComp->ActivateSystem(true);
	inWater = true;
}

void APlayerPawn::OnOverlapEnd(UPrimitiveComponent* overlappedComp, AActor* otherActor, UPrimitiveComponent* otherComp, int32 otherBodyIndex)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, TEXT("OnOverlapEnd Method Called"));

	niagaraComp->Deactivate();
	inWater = false;
}

void APlayerPawn::OnFlipbookAnimEnd()
{

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, TEXT("OnFlipbookAnimEnd Method Called"));

	nonLoopingAnimPlaying = false;
}

bool APlayerPawn::SimplifiedSphereCast(FVector location, float radius)
{
	FHitResult HitResult;

	return UKismetSystemLibrary::SphereTraceSingleForObjects(
		GetWorld(),
		location,
		location,
		radius,
		st_ObjectTypes,
		false,
		st_ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		0.02f
	);
}

void APlayerPawn::UpdateFlipbookOrientation(bool flipSprite)
{
	playerFlipbookComp->SetWorldScale3D(FVector(flipSprite ? -1.0f : 1.0f, 1.0f, 1.0f));
}

void APlayerPawn::DetectCollisionContinuous()
{
	onFeetPos = -(GetActorUpVector() * boxExtents.Z) + GetActorLocation();
	onHeadPos = (GetActorUpVector() * boxExtents.Z) + GetActorLocation();

	onBLeftPos = -(GetActorForwardVector() * boxExtents.X) + GetActorLocation();
	onBRightPos = (GetActorForwardVector() * boxExtents.X) + GetActorLocation();

	FHitResult HitResult;

	onFeet = SimplifiedSphereCast(onFeetPos, boxExtents.Z / 3.0f);
	onHead = SimplifiedSphereCast(onHeadPos, boxExtents.Z / 3.0f);

	onBellyL = SimplifiedSphereCast(onBLeftPos, boxExtents.X / 3.0f);
	onBellyR = SimplifiedSphereCast(onBRightPos, boxExtents.X / 3.0f);

	if (onHead)
	{
		// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Player's upside down on: %s"), *OtherActor->GetName()));
		playerState = ONHEAD;
	}

	if (onBellyL) {
		playerState = ONBELLY_L;
	}

	if (onBellyR)
	{
		// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Player's laying on belly on: %s"), *OtherActor->GetName()));
		playerState = ONBELLY_R;
	}

	if (onFeet)
	{
		// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Player's standing upright on: %s"), *OtherActor->GetName()));
		playerState = ONFEET;
	}
}

void APlayerPawn::SetFlipbookAnimation(UPaperFlipbook* flipbook, bool looping, bool interrupts)
{
	// Stops a new animation from being set if a 
	// non-looping animation is still running unless
	// animation can interrupt

	if (!nonLoopingAnimPlaying || interrupts) {
		playerFlipbookComp->SetFlipbook(flipbook);
		playerFlipbookComp->SetLooping(looping);
		playerFlipbookComp->Play();
		nonLoopingAnimPlaying = !looping;
	}
}

// FUNCTION NOT USED
int APlayerPawn::IsFlipBookPlaying(UPaperFlipbook* flipbook)
{
	if (playerFlipbookComp->GetFlipbook() == flipbook) {
		return (!playerFlipbookComp->IsLooping() && nonLoopingAnimPlaying);
	}

	return -1;
}

float APlayerPawn::CalcAlpha()
{
	FVector v = FVector(GetVelocity().X, GetVelocity().Z, 0.0f);

	FVector forwards = FVector(GetActorUpVector().X, GetActorUpVector().Z, 0.0f);

	float alpha = v.CosineAngle2D(forwards);

	FVector cross = v.CrossProduct(v, forwards);

	if (cross.Z < 0) {
		return -std::acos(alpha);
	}

	return std::acos(alpha);
}

FVector APlayerPawn::CalcHydroLift(float theta)
{
	FVector v = GetVelocity();

	if (GEngine && !isnan(theta)) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White, FString::Printf(TEXT("Calculating sine Theta: %f"), std::sin(theta)));

	FVector lift = FVector(-(v.Z), 0.0f, (v.X)) * std::sin(theta);

	DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + lift,
		FColor(0, 255, 0),
		false, -1, 0,
		5.0f
	);

	return lift;

}
