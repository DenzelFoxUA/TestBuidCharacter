// Fill out your copyright notice in the Description page of Project Settings.

#include "MyControlledChar.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

// Sets default values
AMyControlledChar::AMyControlledChar()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//1 Create collision capsule as a root component
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision Capsule"));
	RootComponent = CollisionCapsule;
	//2 adding col size of capsule 40 - radius, 180 - h
	CollisionCapsule->InitCapsuleSize(40.f, 90.f);
	CollisionCapsule->SetCollisionProfileName(TEXT("PlayerPawn"));

	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Character Mesh"));
	CharacterMesh->SetupAttachment(RootComponent);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FRotator CharacterDirection(0.f, -90.f, 0.f);
	CharacterMesh->SetRelativeRotation(CharacterDirection);

	//3 Adding springarm
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("Spring Arm"));
	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->TargetArmLength = 300.f;
	FVector CameraLocation(0.f, 0.f, 80.f);
	FRotator CameraDirection(0.f, 90.f, 0.f);
	SpringArmComp->SetRelativeLocation(CameraLocation);
	SpringArmComp->bUsePawnControlRotation = true;
	SpringArmComp->SetRelativeRotation(CameraDirection);
	
	//5 create a camera component and attach it to the root component
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(SpringArmComp);
	
	MovementComp = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement component"));
	MovementComp->SetUpdatedComponent(RootComponent);
	MovementComp->MaxSpeed = WalkSpeed;

	CameraComp->bEnableFirstPersonFieldOfView = true;
	CameraComp->bEnableFirstPersonScale = true;
	CameraComp->FirstPersonFieldOfView = 70.f;
	CameraComp->FirstPersonScale = 0.7f;

}

// Called when the game starts or when spawned
void AMyControlledChar::BeginPlay()
{
	Super::BeginPlay();

	MovementComp->MaxSpeed = WalkSpeed;

	CharacterMesh->SetAnimInstanceClass(CharacterAnimation->GeneratedClass);

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(PlayerMappingContext, 0);
		}
	}
}

bool AMyControlledChar::IsOnGround() const
{
	const FVector CapsulePos = CollisionCapsule->GetComponentLocation();
	const float CapsuleR = CollisionCapsule->GetScaledCapsuleRadius();
	const float CapsuleHalfH = CollisionCapsule->GetScaledCapsuleHalfHeight();

	constexpr float CapsuleHeightOffset = 5.f;
	constexpr float GroundTolerance = 3.f;

	const FCollisionShape CollShape =
		FCollisionShape::MakeCapsule(CapsuleR, CapsuleHalfH - CapsuleHeightOffset);

	const FVector Start = CapsulePos;
	const FVector End = Start - FVector(
		0.f, 0.f, CapsuleHeightOffset + GroundTolerance
	);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity, 
		ECC_Visibility, 
		CollShape, 
		QueryParams);

	if (bHit)
	{
		return Hit.ImpactNormal.Z > 0.7f;
	}


	return false;
}

// Called every frame
void AMyControlledChar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsGrounded)
	{
		VerticalVelocity = 0.f;
	}
	else
	{
		VerticalVelocity += GetWorld()->GetGravityZ() * DeltaTime;
	}

	bIsGrounded = VerticalVelocity <= 0.f && IsOnGround();
	const float DeltaZ = VerticalVelocity * DeltaTime;

	FVector Vertical(0.f, 0.f, DeltaZ);

	FHitResult Hit;
	AddActorWorldOffset(Vertical, true, &Hit);

	//перевірка при ударі об стелю або щось зверху
	if (Hit.IsValidBlockingHit())
	{
		if (VerticalVelocity > 0.f && Hit.ImpactNormal.Z < -0.7f)
		{
			VerticalVelocity = 0.f;
		}
		else if (VerticalVelocity <= 0.f && Hit.ImpactNormal.Z >= 0.7f)
		{
			bIsGrounded = true;
		}
	}
}

// Called to bind functionality to input
void AMyControlledChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&AMyControlledChar::Move);

		EnhancedInputComponent->BindAction(
			SprintAction,
			ETriggerEvent::Started,
			this,
			&AMyControlledChar::Sprint);

		EnhancedInputComponent->BindAction(
			SprintAction,
			ETriggerEvent::Completed,
			this,
			&AMyControlledChar::StopSprint);

		EnhancedInputComponent->BindAction(
			LookAction,
			ETriggerEvent::Triggered,
			this,
			&AMyControlledChar::Look);

		EnhancedInputComponent->BindAction(
			ChangeViewAction,
			ETriggerEvent::Started,
			this,
			&AMyControlledChar::SwitchView);

		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Started,
			this,
			&AMyControlledChar::Jump);
		
	}
}

void AMyControlledChar::Move(const FInputActionValue& Value)
{
	const FVector2D MovementValue = Value.Get<FVector2D>();

	if (Controller)
	{
		//це вектор пероснажа
		//const FVector Right = GetActorRightVector();
		//це ротатор контролера для розрахунку напрямку руху відносно камери, тобто куди вона дивитиметься
		const FRotator ControllerRotation = Controller->GetControlRotation();
		//але нам потрібен лише Yaw тому ми свторюємо нульовий ротатор, куди копіюємо значення з конторллера Yaw
		const FRotator YawRotation(0.f, ControllerRotation.Yaw, 0.f);

		//далі ми це переробим під вектор напрямку
		const FRotationMatrix RotMatrix(YawRotation);

		const FVector Forward = RotMatrix.GetUnitAxis(EAxis::X);
		const FVector Right = RotMatrix.GetUnitAxis(EAxis::Y);

		const FVector MoveDirection = Forward * MovementValue.Y + Right * MovementValue.X;
		if (!MoveDirection.IsNearlyZero())
		{
			FRotator MoveRotator = MoveDirection.Rotation();
			SetActorRotation(MoveRotator);
		}

		AddMovementInput(Right, MovementValue.X);
		AddMovementInput(Forward, MovementValue.Y);

	}
}

void AMyControlledChar::Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerPitchInput(LookValue.Y);
		AddControllerYawInput(LookValue.X);
	}
}

void AMyControlledChar::Jump()
{

	if (bIsGrounded)
	{
		VerticalVelocity = JumpVelocity;
		bIsGrounded = false;
	}
}

void AMyControlledChar::Sprint(const FInputActionValue& Value)
{
	MovementComp->MaxSpeed = RunSpeed;
}

void AMyControlledChar::StopSprint(const FInputActionValue& Value)
{
	MovementComp->MaxSpeed = WalkSpeed;
}

void AMyControlledChar::SwitchView()
{
	if (bIsFirstPerson)
	{
		bIsFirstPerson = false;
		SpringArmComp->TargetArmLength = 300.f;
	}
	else
	{
		SpringArmComp->TargetArmLength = 0.f;
		bIsFirstPerson = true;
	}
}
