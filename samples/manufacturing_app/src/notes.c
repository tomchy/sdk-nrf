	/*
	 * Missing pieces from the B0.
	 */

	/* Is BL2 present and valid?
	 * - if not present or not valid - enable authenticated erase all.
	 */

	/* Check the current LCS value:
	 * - A&T: continue to BL2
	 * - check BL2 signature otherwise
	 *   - if signature is valid - continue to BL2
	 *   - otherwise - enable authenticated erase all.
	 */

	/*
	 * Missing pieces from BL2.
	 */

	/* Check LCS. */
	/* Secured - check app candidate presence and valididty.
	 * Otherwise - verify the provisioning image or update candidate (slot 0) digest.
	 * If Assembly and test - jump directly into the provisioning image.
	 * If Secured or Decomissioned - check application signature.
	 *  - if invalid - enable authenticated erase all.
	 *  - if valid - jump to the application.
	 * If PROT Provisioning - check the provisioning image signature.
	 *  - if invalid - enable authenticated erase all.
	 *  - if valid - jump to the provisioning image.
	 */

	/*
	 * Steps to be performed in a trusted manufacturing environment.
	 */

	/* Check the correctness of the B0 (NSIB). */
	/* Check the correctness of the B1 (MCUboot). */
	/* Check the correctness of the provisioning image (self). */
	/* Check the presence of the application update candidate. */
	/* Check the updateability of the application update candidate. */
	/* Check essential fields of the UICR. */

	/* Provision IKG seed. */
	/* Open connection to the external provisioning tool (if needed). */
	/* Provision secure boot keys. */
	/* Provision ADAC keys. */

	/* Configure erase protection.*/
	/* Set LCS to "PROT Provisioning". */
	/* Check erase protection value .*/
	/* Check the current LCS value. */

	/*
	 * Steps to be performed in an untrusted manufacturing environment.
	 */

	/* The the product (PCB, connections, sensors, etc.). */
	/* Open a secure connection for provisioning of other assets. */
	/* Set LCS to "Secured". */
	/* Schedule installation of the application candidate. */
	/* Revoke Manufacturing Tool (provisioning image) verification key. */
	/* Reboot. */

	/* At this point the BL2 (MCUboot) should install the main application.
	 * The update process should overwrite the provisioning image.
	 * As a result, no other actions can or should be performed by this application.
	 */
