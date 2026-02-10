#include "../../Includes.h"

void EntityList::UpdateEntities()
{
	// clear list from previous tick
	// @NOTE: You can optimize this by checking if entity is still valid
	m_vecEntities.clear();

	// Method 1: Try using Entity System (original method)
	bool bEntitiesFound = false;
	if (g_Interfaces.m_GameEntitySystem.m_pFirst)
	{
		for(CEntityIdentity* pEntity = g_Interfaces.m_GameEntitySystem.m_pFirst; pEntity != nullptr; pEntity = pEntity->m_pNext())
		{
			C_BaseEntity* pBaseEntity = reinterpret_cast<C_BaseEntity*>(pEntity->m_pInstance());
			if (!pBaseEntity)
				continue;
			
			bEntitiesFound = true;

			const FNV1A_t uSchemaNameHash = FNV1A::Hash(pBaseEntity->GetSchemaName().c_str());
			std::string strSchemaName = pBaseEntity->GetSchemaName();
		
			switch (uSchemaNameHash)
			{
				case FNV1A::HashConst("CCSPlayerController"):
				{
					m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_PLAYER));
					break;
				}
				case FNV1A::HashConst("C_Chicken"):
				{
					m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_CHICKEN));
					break;
				}
				case FNV1A::HashConst("C_PlantedC4"):
				{
					m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_BOMB));
					break;
				}
				case FNV1A::HashConst("C_BaseCSGrenadeProjectile"):
				case FNV1A::HashConst("C_FlashbangProjectile"):
				case FNV1A::HashConst("C_HEGrenadeProjectile"):
				case FNV1A::HashConst("C_SmokeGrenadeProjectile"):
				case FNV1A::HashConst("C_MolotovProjectile"):
				case FNV1A::HashConst("C_DecoyProjectile"):
				{
					m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_GRENADE));
					break;
				}
				case FNV1A::HashConst("C_BasePlayerWeapon"):
				case FNV1A::HashConst("C_CSWeaponBase"):
				case FNV1A::HashConst("C_CSWeaponBaseGun"):
				case FNV1A::HashConst("C_EconEntity"):
				{
					// Check if this is a dropped C4 weapon (item definition index 49)
					C_BasePlayerWeapon* pWeapon = reinterpret_cast<C_BasePlayerWeapon*>(pBaseEntity);
					if (pWeapon)
					{
						try
						{
							// Check if entity has a valid position (is dropped, not held)
							CGameSceneNode* pSceneNode = pWeapon->m_pGameSceneNode();
							if (pSceneNode)
							{
								Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
								// Only consider it dropped if it has a valid position (not zero)
								if (vecOrigin.x != 0.0f || vecOrigin.y != 0.0f || vecOrigin.z != 0.0f)
								{
									std::uint16_t nItemDefIndex = pWeapon->GetItemDefinitionIndex();
									if (nItemDefIndex == 49) // weapon_c4
									{
										m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_DROPPED_BOMB));
									}
								}
							}
						}
						catch (...)
						{
							// Skip if we can't read the item definition index
						}
					}
					break;
				}
			}
			
			// Also check all entities that might be weapons or C4 (fallback for different schema names)
			// Check if schema name contains "Weapon", "Econ", or "C4" (but not "PlantedC4" which we already handle)
			if (strSchemaName.find("PlantedC4") == std::string::npos) // Skip if it's a planted bomb (already handled)
			{
				if (strSchemaName.find("Weapon") != std::string::npos || 
				    strSchemaName.find("Econ") != std::string::npos ||
				    strSchemaName.find("C4") != std::string::npos)
				{
					// Try to cast to weapon and check if it's C4
					C_BasePlayerWeapon* pWeapon = reinterpret_cast<C_BasePlayerWeapon*>(pBaseEntity);
					if (pWeapon)
					{
						try
						{
							CGameSceneNode* pSceneNode = pWeapon->m_pGameSceneNode();
							if (pSceneNode)
							{
								Vector vecOrigin = pSceneNode->m_vecAbsOrigin();
								// Only consider it dropped if it has a valid position
								if (vecOrigin.x != 0.0f || vecOrigin.y != 0.0f || vecOrigin.z != 0.0f)
								{
									// Check if it's C4 by item definition index
									std::uint16_t nItemDefIndex = pWeapon->GetItemDefinitionIndex();
									if (nItemDefIndex == 49) // weapon_c4
									{
										// Check if we haven't already added this entity
										bool bAlreadyAdded = false;
										for (const auto& existing : m_vecEntities)
										{
											if (existing.m_pEntity == pBaseEntity && 
											    (existing.m_eType == EEntityType::ENTITY_DROPPED_BOMB || existing.m_eType == EEntityType::ENTITY_BOMB))
											{
												bAlreadyAdded = true;
												break;
											}
										}
										if (!bAlreadyAdded)
										{
											m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_DROPPED_BOMB));
										}
									}
									// Also check by schema name if it contains "C4" (for cases where item def index might not work)
									else if (strSchemaName.find("C4") != std::string::npos)
									{
										// Check if we haven't already added this entity
										bool bAlreadyAdded = false;
										for (const auto& existing : m_vecEntities)
										{
											if (existing.m_pEntity == pBaseEntity && 
											    (existing.m_eType == EEntityType::ENTITY_DROPPED_BOMB || existing.m_eType == EEntityType::ENTITY_BOMB))
											{
												bAlreadyAdded = true;
												break;
											}
										}
										if (!bAlreadyAdded)
										{
											m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_DROPPED_BOMB));
										}
									}
								}
							}
						}
						catch (...)
						{
							// Skip if we can't read
						}
					}
				}
			}
		}
	}
	
	// Method 2: Fallback - Use Entity List directly if Entity System method failed or found no entities
	if (!bEntitiesFound && g_Globals.m_uEntityList != 0)
	{
		// Iterate through entity list using GetBaseEntity method
		// CS2 typically has entities from index 1 to ~64 for players, higher for other entities
		for (int i = 1; i < 1024; i++)
		{
			C_BaseEntity* pBaseEntity = C_BaseEntity::GetBaseEntity(i);
			if (!pBaseEntity)
				continue;
			
			try
			{
				const FNV1A_t uSchemaNameHash = FNV1A::Hash(pBaseEntity->GetSchemaName().c_str());
				std::string strSchemaName = pBaseEntity->GetSchemaName();
				
				// Only process player controllers for ESP
				if (uSchemaNameHash == FNV1A::HashConst("CCSPlayerController"))
				{
					// Check if we already added this entity
					bool bAlreadyAdded = false;
					for (const auto& existing : m_vecEntities)
					{
						if (existing.m_pEntity == pBaseEntity)
						{
							bAlreadyAdded = true;
							break;
						}
					}
					
					if (!bAlreadyAdded)
					{
						m_vecEntities.emplace_back(EntityObject_t(pBaseEntity, pBaseEntity->GetRefEHandle().GetEntryIndex(), EEntityType::ENTITY_PLAYER));
					}
				}
			}
			catch (...)
			{
				// Skip invalid entities
				continue;
			}
		}
	}
}