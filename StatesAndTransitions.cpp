#include "stdafx.h"
#include "StatesAndTransitions.h"
#include "Agent.h"
#include "GOAPPlanner.h"

// STATES
// ------------------------------------------------------------------------------------------------------------------------------------------------
/// IdleState: public FSMState
void IdleState::OnEnter(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard)
{
	ResetIdleState();

	// The last action encountered a problem with fullfilling it's effects. Replan!
	if (pPlanner->GetEncounteredProblem() == true)
	{
		pPlanner->SetEncounteredProblem(false);
		m_ReplanActions = true;
		return;
	}

	// Check if we came from an action
	GOAPAction* pAction = pPlanner->GetAction();
	if (pAction)
	{
		pPlanner->NextAction();
		if (pPlanner->GetAction())
		{
			// Next action exists
			m_HasNext = true;
			std::cout << "Next action chosen, currentAction: " << pPlanner->GetAction()->ToString()<<"\n";
		}
	}
}
void IdleState::Update(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard, float deltaTime)
{
	// Don't need a new plan if we have a next action
	if (m_HasNext)
		return;

	// Only plan actions every x seconds
	if (m_ActionTimer > m_RefreshActionTime)
	{
		// Reset action timer
		m_ActionTimer = 0.f;

		// Plan the action until one is found
		pPlanner->PlanAction();

		std::cout << "Planned actions, currentAction: " << pPlanner->GetAction()->ToString()<<"\n";
	}
	else
		m_ActionTimer += deltaTime;
}
void IdleState::ResetIdleState()
{
	m_ActionTimer = m_RefreshActionTime;
	m_HasNext = false;
	m_ReplanActions = false;
}

// GoToState: public FSMState
void GoToState::OnEnter(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard)
{
	// Debug announcement
	//std::cout << "Entered GoToState\n";
	// Setup the action
	pPlanner->GetAction()->Setup(pInterface, pPlanner, pBlackboard);

	// Setup GoTo state
	m_PathRefreshTimer = m_PathRefreshDuration;

	// Get the agent
	Agent* pAgent = nullptr;
	bool foundData = pBlackboard->GetData("Agent", pAgent);
	if (!foundData)
	{
		std::cout << "GoToState::OnEnter, problem fetching data from blackboard\n";
		return;
	}

	// Set agent behavior
	pAgent->SetBehavior(BehaviorType::SEEK);
}
void GoToState::OnExit(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard)
{
	// Get the agent
	Agent* pAgent = nullptr;
	bool foundData = pBlackboard->GetData("Agent", pAgent);
	if (!foundData)
	{
		std::cout << "GoToState::OnExit, problem fetching data from blackboard\n";
		return;
	}

	// Set agent behavior
	pAgent->SetBehavior(BehaviorType::NONE);
}
void GoToState::Update(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard, float deltaTime)
{
	Agent* pAgent = nullptr;
	bool foundData = pBlackboard->GetData("Agent", pAgent);
	if (!foundData)
	{
		std::cout << "GoToState::Update, problem fetching data from blackboard\n";
		return;
	}

	AgentInfo agentInfo = pInterface->Agent_GetInfo();
	Elite::Vector2 currentAgentPosition = agentInfo.Position;

	// Only fetch a new path every x frames
	if (m_PathRefreshTimer >= m_PathRefreshDuration)
	{
		m_PathRefreshTimer = 0.f;
		Elite::Vector2 closestNode = pInterface->NavMesh_GetClosestPathPoint(pPlanner->GetAction()->GetMoveLocation());
		pAgent->SetSeekPos(closestNode);
	}
	m_PathRefreshTimer += deltaTime;

	pInterface->Draw_SolidCircle(pPlanner->GetAction()->GetMoveLocation(), .5f, Elite::Vector2{}, Elite::Vector3{ 0.f, 1.f, 0.f });
}

// Perform state: public FSMState
void PerformState::OnEnter(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard)
{
	//std::cout << "Entered PerformState\n";
	// Setup the action
	pPlanner->GetAction()->Setup(pInterface, pPlanner, pBlackboard);
}
void PerformState::Update(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard, float deltaTime)
{
	// Perform until the action is done
	bool performed = pPlanner->GetAction()->Perform(pInterface, pPlanner, pBlackboard, deltaTime);

	// Check if the perform encountered a problem
	if (!performed)
	{
		// Let the planner know a problem was encountered so we can replan (handled in PerformedTransition::ToTransition)
		pPlanner->SetEncounteredProblem(true);
	}
}
/// ------------------------------------------------------------------------------------------------------------------------------------------------


// TRANSITIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------
// GoToTransition: public FSMTransition
bool GoToTransition::ToTransition(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard) const
{
	// Only start moving towards a point if the action requires movement
	GOAPAction* pAction = pPlanner->GetAction();

	// Action required
	if (!pAction)
		return false;

	// Only transition to goto if movement is required
	if (pAction->RequiresMovement(pInterface, pPlanner, pBlackboard))
		return true;

	return false;
}

// PerformTransition: public FSMTransition
bool PerformTransition::ToTransition(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard) const
{
	GOAPAction* pAction = pPlanner->GetAction();

	// Action required
	if (!pAction)
		return false;

	// Only transition to perform if no more movement is required
	if (pAction->RequiresMovement(pInterface, pPlanner, pBlackboard))
		return false;

	return true;
}

// PerformCompleteTransition: public FSMTransition
bool PerformedTransition::ToTransition(IExamInterface* pInterface, GOAPPlanner* pPlanner, Blackboard* pBlackboard) const
{
	// Complete the action early if a problem was encountered
	if (pPlanner->GetEncounteredProblem() == true)
		return true;

	GOAPAction* pAction = pPlanner->GetAction();

	if (!pAction)
		return false;

	// Keep doing the action until it is done
	if (pAction->IsDone(pInterface, pPlanner, pBlackboard)) {
		std::cout << "Action performed\n\n";
		return true;
	}

	return false;
}
/// ------------------------------------------------------------------------------------------------------------------------------------------------