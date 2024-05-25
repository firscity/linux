// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe RC driver for Synopsys DesignWare Core
 *
 * Copyright (C) 2015-2016 Synopsys, Inc. (www.synopsys.com)
 *
 * Authors: Joao Pinto <Joao.Pinto@synopsys.com>
 */

/*** PCIe Designware Header ***/
#ifndef _PCIE_DESIGNWARE_H
#define _PCIE_DESIGNWARE_H

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/pci_regs.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/resource.h>
#include <linux/types.h>
#include <linux/regmap.h>

#include "../../pci.h"
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>

/* Parameters for the waiting for link up routine */
#define LINK_WAIT_MAX_RETRIES		10
#define LINK_WAIT_USLEEP_MIN		90000
#define LINK_WAIT_USLEEP_MAX		100000

/* Parameters for the waiting for iATU enabled routine */
#define LINK_WAIT_MAX_IATU_RETRIES	5
#define LINK_WAIT_IATU			9

/* Synopsys-specific PCIe configuration registers */
#define PCIE_PORT_AFR			0x70C
#define PORT_AFR_N_FTS_MASK		GENMASK(15, 8)
#define PORT_AFR_N_FTS(n)		FIELD_PREP(PORT_AFR_N_FTS_MASK, n)
#define PORT_AFR_CC_N_FTS_MASK		GENMASK(23, 16)
#define PORT_AFR_CC_N_FTS(n)		FIELD_PREP(PORT_AFR_CC_N_FTS_MASK, n)
#define PORT_AFR_ENTER_ASPM		BIT(30)
#define PORT_AFR_L0S_ENTRANCE_LAT_SHIFT	24
#define PORT_AFR_L0S_ENTRANCE_LAT_MASK	GENMASK(26, 24)
#define PORT_AFR_L1_ENTRANCE_LAT_SHIFT	27
#define PORT_AFR_L1_ENTRANCE_LAT_MASK	GENMASK(29, 27)

#define PCIE_PORT_LINK_CONTROL		0x710
#define PORT_LINK_DLL_LINK_EN		BIT(5)
#define PORT_LINK_FAST_LINK_MODE	BIT(7)
#define PORT_LINK_MODE_MASK		GENMASK(21, 16)
#define PORT_LINK_MODE(n)		FIELD_PREP(PORT_LINK_MODE_MASK, n)
#define PORT_LINK_MODE_1_LANES		PORT_LINK_MODE(0x1)
#define PORT_LINK_MODE_2_LANES		PORT_LINK_MODE(0x3)
#define PORT_LINK_MODE_4_LANES		PORT_LINK_MODE(0x7)
#define PORT_LINK_MODE_8_LANES		PORT_LINK_MODE(0xf)

#define PCIE_PORT_DEBUG0		0x728
#define PORT_LOGIC_LTSSM_STATE_MASK	0x1f
#define PORT_LOGIC_LTSSM_STATE_L0	0x11
#define PCIE_PORT_DEBUG1		0x72C
#define PCIE_PORT_DEBUG1_LINK_UP		BIT(4)
#define PCIE_PORT_DEBUG1_LINK_IN_TRAINING	BIT(29)

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C
#define PORT_LOGIC_N_FTS_MASK		GENMASK(7, 0)
#define PORT_LOGIC_SPEED_CHANGE		BIT(17)
#define PORT_LOGIC_LINK_WIDTH_MASK	GENMASK(12, 8)
#define PORT_LOGIC_LINK_WIDTH(n)	FIELD_PREP(PORT_LOGIC_LINK_WIDTH_MASK, n)
#define PORT_LOGIC_LINK_WIDTH_1_LANES	PORT_LOGIC_LINK_WIDTH(0x1)
#define PORT_LOGIC_LINK_WIDTH_2_LANES	PORT_LOGIC_LINK_WIDTH(0x2)
#define PORT_LOGIC_LINK_WIDTH_4_LANES	PORT_LOGIC_LINK_WIDTH(0x4)
#define PORT_LOGIC_LINK_WIDTH_8_LANES	PORT_LOGIC_LINK_WIDTH(0x8)

#define PCIE_MSI_ADDR_LO		0x820
#define PCIE_MSI_ADDR_HI		0x824
#define PCIE_MSI_INTR0_ENABLE		0x828
#define PCIE_MSI_INTR0_MASK		0x82C
#define PCIE_MSI_INTR0_STATUS		0x830

#define PCIE_PORT_MULTI_LANE_CTRL	0x8C0
#define PORT_MLTI_UPCFG_SUPPORT		BIT(7)

#define PCIE_ATU_VIEWPORT		0x900
#define PCIE_ATU_REGION_INBOUND		BIT(31)
#define PCIE_ATU_REGION_OUTBOUND	0
#define PCIE_ATU_REGION_INDEX2		0x2
#define PCIE_ATU_REGION_INDEX1		0x1
#define PCIE_ATU_REGION_INDEX0		0x0
#define PCIE_ATU_CR1			0x904
#define PCIE_ATU_TYPE_MEM		0x0
#define PCIE_ATU_TYPE_IO		0x2
#define PCIE_ATU_TYPE_CFG0		0x4
#define PCIE_ATU_TYPE_CFG1		0x5
#define PCIE_ATU_FUNC_NUM(pf)           ((pf) << 20)
#define PCIE_ATU_CR2			0x908
#define PCIE_ATU_ENABLE			BIT(31)
#define PCIE_ATU_BAR_MODE_ENABLE	BIT(30)
#define PCIE_ATU_FUNC_NUM_MATCH_EN      BIT(19)
#define PCIE_ATU_LOWER_BASE		0x90C
#define PCIE_ATU_UPPER_BASE		0x910
#define PCIE_ATU_LIMIT			0x914
#define PCIE_ATU_LOWER_TARGET		0x918
#define PCIE_ATU_BUS(x)			FIELD_PREP(GENMASK(31, 24), x)
#define PCIE_ATU_DEV(x)			FIELD_PREP(GENMASK(23, 19), x)
#define PCIE_ATU_FUNC(x)		FIELD_PREP(GENMASK(18, 16), x)
#define PCIE_ATU_UPPER_TARGET		0x91C

#define PCIE_MISC_CONTROL_1_OFF		0x8BC
#define PCIE_DBI_RO_WR_EN		BIT(0)

#define PCIE_MSIX_DOORBELL		0x948
#define PCIE_MSIX_DOORBELL_PF_SHIFT	24

#define PCIE_PL_CHK_REG_CONTROL_STATUS			0xB20
#define PCIE_PL_CHK_REG_CHK_REG_START			BIT(0)
#define PCIE_PL_CHK_REG_CHK_REG_CONTINUOUS		BIT(1)
#define PCIE_PL_CHK_REG_CHK_REG_COMPARISON_ERROR	BIT(16)
#define PCIE_PL_CHK_REG_CHK_REG_LOGIC_ERROR		BIT(17)
#define PCIE_PL_CHK_REG_CHK_REG_COMPLETE		BIT(18)

#define PCIE_PL_CHK_REG_ERR_ADDR			0xB28

/*
 * iATU Unroll-specific register definitions
 * From 4.80 core version the address translation will be made by unroll
 */
#define PCIE_ATU_UNR_REGION_CTRL1	0x00
#define PCIE_ATU_UNR_REGION_CTRL2	0x04
#define PCIE_ATU_UNR_LOWER_BASE		0x08
#define PCIE_ATU_UNR_UPPER_BASE		0x0C
#define PCIE_ATU_UNR_LOWER_LIMIT	0x10
#define PCIE_ATU_UNR_LOWER_TARGET	0x14
#define PCIE_ATU_UNR_UPPER_TARGET	0x18
#define PCIE_ATU_UNR_UPPER_LIMIT	0x20

/*
 * The default address offset between dbi_base and atu_base. Root controller
 * drivers are not required to initialize atu_base if the offset matches this
 * default; the driver core automatically derives atu_base from dbi_base using
 * this offset, if atu_base not set.
 */
#define DEFAULT_DBI_ATU_OFFSET (0x3 << 20)

/* Register address builder */
#define PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(region) \
		((region) << 9)

#define PCIE_GET_ATU_INB_UNR_REG_OFFSET(region) \
		(((region) << 9) | BIT(8))

#define MAX_MSI_IRQS			256
#define MAX_MSI_IRQS_PER_CTRL		32
#define MAX_MSI_CTRLS			(MAX_MSI_IRQS / MAX_MSI_IRQS_PER_CTRL)
#define MSI_REG_CTRL_BLOCK_SIZE		12
#define MSI_DEF_NUM_VECTORS		32

/* Maximum number of inbound/outbound iATUs */
#define MAX_IATU_IN			256
#define MAX_IATU_OUT			256

struct pcie_port;
struct dw_pcie6;
struct dw_pcie6_ep;

enum dw_pcie6_region_type {
	DW_PCIE_REGION_UNKNOWN,
	DW_PCIE_REGION_INBOUND,
	DW_PCIE_REGION_OUTBOUND,
};

enum dw_pcie6_device_mode {
	DW_PCIE_UNKNOWN_TYPE,
	DW_PCIE_EP_TYPE,
	DW_PCIE_LEG_EP_TYPE,
	DW_PCIE_RC_TYPE,
};

struct dw_pcie6_host_ops {
	int (*host_init)(struct pcie_port *pp);
	void (*set_num_vectors)(struct pcie_port *pp);
	int (*msi_host_init)(struct pcie_port *pp);
};

struct pcie_port {
	u64			cfg0_base;
	void __iomem		*va_cfg0_base;
	u32			cfg0_size;
	resource_size_t		io_base;
	phys_addr_t		io_bus_addr;
	u32			io_size;
	int			irq;
	const struct dw_pcie6_host_ops *ops;
	int			msi_irq;
	struct irq_domain	*irq_domain;
	struct irq_domain	*msi_domain;
	u16			msi_msg;
	dma_addr_t		msi_data;
	struct irq_chip		*msi_irq_chip;
	u32			num_vectors;
	u32			irq_mask[MAX_MSI_CTRLS];
	struct pci_host_bridge  *bridge;
	raw_spinlock_t		lock;
	DECLARE_BITMAP(msi_irq_in_use, MAX_MSI_IRQS);
};

enum dw_pcie6_as_type {
	DW_PCIE_AS_UNKNOWN,
	DW_PCIE_AS_MEM,
	DW_PCIE_AS_IO,
};

struct dw_pcie6_ep_ops {
	void	(*ep_init)(struct dw_pcie6_ep *ep);
	int	(*raise_irq)(struct dw_pcie6_ep *ep, u8 func_no,
			     enum pci_epc_irq_type type, u16 interrupt_num);
	const struct pci_epc_features* (*get_features)(struct dw_pcie6_ep *ep);
	/*
	 * Provide a method to implement the different func config space
	 * access for different platform, if different func have different
	 * offset, return the offset of func. if use write a register way
	 * return a 0, and implement code in callback function of platform
	 * driver.
	 */
	unsigned int (*func_conf_select)(struct dw_pcie6_ep *ep, u8 func_no);
};

struct dw_pcie6_ep_func {
	struct list_head	list;
	u8			func_no;
	u8			msi_cap;	/* MSI capability offset */
	u8			msix_cap;	/* MSI-X capability offset */
};

struct dw_pcie6_ep {
	struct pci_epc		*epc;
	struct list_head	func_list;
	const struct dw_pcie6_ep_ops *ops;
	phys_addr_t		phys_base;
	size_t			addr_size;
	size_t			page_size;
	u8			bar_to_atu[PCI_STD_NUM_BARS];
	phys_addr_t		*outbound_addr;
	unsigned long		*ib_window_map;
	unsigned long		*ob_window_map;
	u32			num_ib_windows;
	u32			num_ob_windows;
	void __iomem		*msi_mem;
	phys_addr_t		msi_mem_phys;
	struct pci_epf_bar	*epf_bar[PCI_STD_NUM_BARS];
};

struct dw_pcie6_ops {
	u64	(*cpu_addr_fixup)(struct dw_pcie6 *pcie, u64 cpu_addr);
	u32	(*read_dbi)(struct dw_pcie6 *pcie, void __iomem *base, u32 reg,
			    size_t size);
	void	(*write_dbi)(struct dw_pcie6 *pcie, void __iomem *base, u32 reg,
			     size_t size, u32 val);
	void    (*write_dbi2)(struct dw_pcie6 *pcie, void __iomem *base, u32 reg,
			      size_t size, u32 val);
	int	(*link_up)(struct dw_pcie6 *pcie);
	int	(*start_link)(struct dw_pcie6 *pcie);
	void	(*stop_link)(struct dw_pcie6 *pcie);
};

struct dw_pcie6 {
	struct device		*dev;
	void __iomem		*dbi_base;
	void __iomem		*dbi_base2;
	/* Used when iatu_unroll_enabled is true */
	void __iomem		*atu_base;
	u32			num_viewport;
	u8			iatu_unroll_enabled;
	struct pcie_port	pp;
	struct dw_pcie6_ep	ep;
	const struct dw_pcie6_ops *ops;
	unsigned int		version;
	int			num_lanes;
	int			link_gen;
	u8			n_fts[2];
};

#define to_dw_pcie6_from_pp(port) container_of((port), struct dw_pcie6, pp)

#define to_dw_pcie6_from_ep(endpoint)   \
		container_of((endpoint), struct dw_pcie6, ep)

u8 dw_pcie6_find_capability(struct dw_pcie6 *pci, u8 cap);
u16 dw_pcie6_find_ext_capability(struct dw_pcie6 *pci, u8 cap);

int dw_pcie6_read(void __iomem *addr, int size, u32 *val);
int dw_pcie6_write(void __iomem *addr, int size, u32 val);

u32 dw_pcie6_read_dbi(struct dw_pcie6 *pci, u32 reg, size_t size);
void dw_pcie6_write_dbi(struct dw_pcie6 *pci, u32 reg, size_t size, u32 val);
void dw_pcie6_write_dbi2(struct dw_pcie6 *pci, u32 reg, size_t size, u32 val);
int dw_pcie6_link_up(struct dw_pcie6 *pci);
void dw_pcie6_upconfig_setup(struct dw_pcie6 *pci);
int dw_pcie6_wait_for_link(struct dw_pcie6 *pci);
void dw_pcie6_prog_outbound_atu(struct dw_pcie6 *pci, int index,
			       int type, u64 cpu_addr, u64 pci_addr,
			       u32 size);
void dw_pcie6_prog_ep_outbound_atu(struct dw_pcie6 *pci, u8 func_no, int index,
				  int type, u64 cpu_addr, u64 pci_addr,
				  u32 size);
int dw_pcie6_prog_inbound_atu(struct dw_pcie6 *pci, u8 func_no, int index,
			     int bar, u64 cpu_addr,
			     enum dw_pcie6_as_type as_type);
void dw_pcie6_disable_atu(struct dw_pcie6 *pci, int index,
			 enum dw_pcie6_region_type type);
void dw_pcie6_setup(struct dw_pcie6 *pci);

static inline void dw_pcie6_writel_dbi(struct dw_pcie6 *pci, u32 reg, u32 val)
{
	dw_pcie6_write_dbi(pci, reg, 0x4, val);
}

static inline u32 dw_pcie6_readl_dbi(struct dw_pcie6 *pci, u32 reg)
{
	return dw_pcie6_read_dbi(pci, reg, 0x4);
}

static inline void dw_pcie6_writew_dbi(struct dw_pcie6 *pci, u32 reg, u16 val)
{
	dw_pcie6_write_dbi(pci, reg, 0x2, val);
}

static inline u16 dw_pcie6_readw_dbi(struct dw_pcie6 *pci, u32 reg)
{
	return dw_pcie6_read_dbi(pci, reg, 0x2);
}

static inline void dw_pcie6_writeb_dbi(struct dw_pcie6 *pci, u32 reg, u8 val)
{
	dw_pcie6_write_dbi(pci, reg, 0x1, val);
}

static inline u8 dw_pcie6_readb_dbi(struct dw_pcie6 *pci, u32 reg)
{
	return dw_pcie6_read_dbi(pci, reg, 0x1);
}

static inline void dw_pcie6_writel_dbi2(struct dw_pcie6 *pci, u32 reg, u32 val)
{
	dw_pcie6_write_dbi2(pci, reg, 0x4, val);
}

static inline void dw_pcie6_dbi_ro_wr_en(struct dw_pcie6 *pci)
{
	u32 reg;
	u32 val;

	reg = PCIE_MISC_CONTROL_1_OFF;
	val = dw_pcie6_readl_dbi(pci, reg);
	val |= PCIE_DBI_RO_WR_EN;
	dw_pcie6_writel_dbi(pci, reg, val);
}

static inline void dw_pcie6_dbi_ro_wr_dis(struct dw_pcie6 *pci)
{
	u32 reg;
	u32 val;

	reg = PCIE_MISC_CONTROL_1_OFF;
	val = dw_pcie6_readl_dbi(pci, reg);
	val &= ~PCIE_DBI_RO_WR_EN;
	dw_pcie6_writel_dbi(pci, reg, val);
}

#ifdef CONFIG_PCIE_DW_HOST
irqreturn_t dw_handle_msi_irq(struct pcie_port *pp);
void dw_pcie6_msi_init(struct pcie_port *pp);
void dw_pcie6_free_msi(struct pcie_port *pp);
void dw_pcie6_setup_rc(struct pcie_port *pp);
int dw_pcie6_host_init(struct pcie_port *pp);
void dw_pcie6_host_deinit(struct pcie_port *pp);
int dw_pcie6_allocate_domains(struct pcie_port *pp);
void __iomem *dw_pcie6_own_conf_map_bus(struct pci_bus *bus, unsigned int devfn,
				       int where);
#else
static inline irqreturn_t dw_handle_msi_irq(struct pcie_port *pp)
{
	return IRQ_NONE;
}

static inline void dw_pcie6_msi_init(struct pcie_port *pp)
{
}

static inline void dw_pcie6_free_msi(struct pcie_port *pp)
{
}

static inline void dw_pcie6_setup_rc(struct pcie_port *pp)
{
}

static inline int dw_pcie6_host_init(struct pcie_port *pp)
{
	return 0;
}

static inline void dw_pcie6_host_deinit(struct pcie_port *pp)
{
}

static inline int dw_pcie6_allocate_domains(struct pcie_port *pp)
{
	return 0;
}
static inline void __iomem *dw_pcie6_own_conf_map_bus(struct pci_bus *bus,
						     unsigned int devfn,
						     int where)
{
	return NULL;
}
#endif

#ifdef CONFIG_PCIE_DW_EP
void dw_pcie6_ep_linkup(struct dw_pcie6_ep *ep);
int dw_pcie6_ep_init(struct dw_pcie6_ep *ep);
int dw_pcie6_ep_init_complete(struct dw_pcie6_ep *ep);
void dw_pcie6_ep_init_notify(struct dw_pcie6_ep *ep);
void dw_pcie6_ep_exit(struct dw_pcie6_ep *ep);
int dw_pcie6_ep_raise_legacy_irq(struct dw_pcie6_ep *ep, u8 func_no);
int dw_pcie6_ep_raise_msi_irq(struct dw_pcie6_ep *ep, u8 func_no,
			     u8 interrupt_num);
int dw_pcie6_ep_raise_msix_irq(struct dw_pcie6_ep *ep, u8 func_no,
			     u16 interrupt_num);
int dw_pcie6_ep_raise_msix_irq_doorbell(struct dw_pcie6_ep *ep, u8 func_no,
				       u16 interrupt_num);
void dw_pcie6_ep_reset_bar(struct dw_pcie6 *pci, enum pci_barno bar);
struct dw_pcie6_ep_func *
dw_pcie6_ep_get_func_from_ep(struct dw_pcie6_ep *ep, u8 func_no);
#else
static inline void dw_pcie6_ep_linkup(struct dw_pcie6_ep *ep)
{
}

static inline int dw_pcie6_ep_init(struct dw_pcie6_ep *ep)
{
	return 0;
}

static inline int dw_pcie6_ep_init_complete(struct dw_pcie6_ep *ep)
{
	return 0;
}

static inline void dw_pcie6_ep_init_notify(struct dw_pcie6_ep *ep)
{
}

static inline void dw_pcie6_ep_exit(struct dw_pcie6_ep *ep)
{
}

static inline int dw_pcie6_ep_raise_legacy_irq(struct dw_pcie6_ep *ep, u8 func_no)
{
	return 0;
}

static inline int dw_pcie6_ep_raise_msi_irq(struct dw_pcie6_ep *ep, u8 func_no,
					   u8 interrupt_num)
{
	return 0;
}

static inline int dw_pcie6_ep_raise_msix_irq(struct dw_pcie6_ep *ep, u8 func_no,
					   u16 interrupt_num)
{
	return 0;
}

static inline int dw_pcie6_ep_raise_msix_irq_doorbell(struct dw_pcie6_ep *ep,
						     u8 func_no,
						     u16 interrupt_num)
{
	return 0;
}

static inline void dw_pcie6_ep_reset_bar(struct dw_pcie6 *pci, enum pci_barno bar)
{
}

static inline struct dw_pcie6_ep_func *
dw_pcie6_ep_get_func_from_ep(struct dw_pcie6_ep *ep, u8 func_no)
{
	return NULL;
}
#endif
#endif /* _PCIE_DESIGNWARE_H */

/*** PCIe Designware Core ***/

/*
 * These interfaces resemble the pci_find_*capability() interfaces, but these
 * are for configuring host controllers, which are bridges *to* PCI devices but
 * are not PCI devices themselves.
 */
static u8 __dw_pcie6_find_next_cap(struct dw_pcie6 *pci, u8 cap_ptr,
				  u8 cap)
{
	u8 cap_id, next_cap_ptr;
	u16 reg;

	if (!cap_ptr)
		return 0;

	reg = dw_pcie6_readw_dbi(pci, cap_ptr);
	cap_id = (reg & 0x00ff);

	if (cap_id > PCI_CAP_ID_MAX)
		return 0;

	if (cap_id == cap)
		return cap_ptr;

	next_cap_ptr = (reg & 0xff00) >> 8;
	return __dw_pcie6_find_next_cap(pci, next_cap_ptr, cap);
}

u8 dw_pcie6_find_capability(struct dw_pcie6 *pci, u8 cap)
{
	u8 next_cap_ptr;
	u16 reg;

	reg = dw_pcie6_readw_dbi(pci, PCI_CAPABILITY_LIST);
	next_cap_ptr = (reg & 0x00ff);

	return __dw_pcie6_find_next_cap(pci, next_cap_ptr, cap);
}

static u16 dw_pcie6_find_next_ext_capability(struct dw_pcie6 *pci, u16 start,
					    u8 cap)
{
	u32 header;
	int ttl;
	int pos = PCI_CFG_SPACE_SIZE;

	/* minimum 8 bytes per capability */
	ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

	if (start)
		pos = start;

	header = dw_pcie6_readl_dbi(pci, pos);
	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap && pos != start)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;

		header = dw_pcie6_readl_dbi(pci, pos);
	}

	return 0;
}

u16 dw_pcie6_find_ext_capability(struct dw_pcie6 *pci, u8 cap)
{
	return dw_pcie6_find_next_ext_capability(pci, 0, cap);
}

int dw_pcie6_read(void __iomem *addr, int size, u32 *val)
{
	if (!IS_ALIGNED((uintptr_t)addr, size)) {
		*val = 0;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	if (size == 4) {
		*val = readl(addr);
	} else if (size == 2) {
		*val = readw(addr);
	} else if (size == 1) {
		*val = readb(addr);
	} else {
		*val = 0;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	return PCIBIOS_SUCCESSFUL;
}

int dw_pcie6_write(void __iomem *addr, int size, u32 val)
{
	if (!IS_ALIGNED((uintptr_t)addr, size))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (size == 4)
		writel(val, addr);
	else if (size == 2)
		writew(val, addr);
	else if (size == 1)
		writeb(val, addr);
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}

u32 dw_pcie6_read_dbi(struct dw_pcie6 *pci, u32 reg, size_t size)
{
	int ret;
	u32 val;

	if (pci->ops->read_dbi)
		return pci->ops->read_dbi(pci, pci->dbi_base, reg, size);

	ret = dw_pcie6_read(pci->dbi_base + reg, size, &val);
	if (ret)
		dev_err(pci->dev, "Read DBI address failed\n");

	return val;
}

void dw_pcie6_write_dbi(struct dw_pcie6 *pci, u32 reg, size_t size, u32 val)
{
	int ret;

	if (pci->ops->write_dbi) {
		pci->ops->write_dbi(pci, pci->dbi_base, reg, size, val);
		return;
	}

	ret = dw_pcie6_write(pci->dbi_base + reg, size, val);
	if (ret)
		dev_err(pci->dev, "Write DBI address failed\n");
}

void dw_pcie6_write_dbi2(struct dw_pcie6 *pci, u32 reg, size_t size, u32 val)
{
	int ret;

	if (pci->ops->write_dbi2) {
		pci->ops->write_dbi2(pci, pci->dbi_base2, reg, size, val);
		return;
	}

	ret = dw_pcie6_write(pci->dbi_base2 + reg, size, val);
	if (ret)
		dev_err(pci->dev, "write DBI address failed\n");
}

static u32 dw_pcie6_readl_atu(struct dw_pcie6 *pci, u32 reg)
{
	int ret;
	u32 val;

	if (pci->ops->read_dbi)
		return pci->ops->read_dbi(pci, pci->atu_base, reg, 4);

	ret = dw_pcie6_read(pci->atu_base + reg, 4, &val);
	if (ret)
		dev_err(pci->dev, "Read ATU address failed\n");

	return val;
}

static void dw_pcie6_writel_atu(struct dw_pcie6 *pci, u32 reg, u32 val)
{
	int ret;

	if (pci->ops->write_dbi) {
		pci->ops->write_dbi(pci, pci->atu_base, reg, 4, val);
		return;
	}

	ret = dw_pcie6_write(pci->atu_base + reg, 4, val);
	if (ret)
		dev_err(pci->dev, "Write ATU address failed\n");
}

static u32 dw_pcie6_readl_ob_unroll(struct dw_pcie6 *pci, u32 index, u32 reg)
{
	u32 offset = PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(index);

	return dw_pcie6_readl_atu(pci, offset + reg);
}

static void dw_pcie6_writel_ob_unroll(struct dw_pcie6 *pci, u32 index, u32 reg,
				     u32 val)
{
	u32 offset = PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(index);

	dw_pcie6_writel_atu(pci, offset + reg, val);
}

static void dw_pcie6_prog_outbound_atu_unroll(struct dw_pcie6 *pci, u8 func_no,
					     int index, int type,
					     u64 cpu_addr, u64 pci_addr,
					     u32 size)
{
	u32 retries, val;
	u64 limit_addr = cpu_addr + size - 1;

	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_LOWER_BASE,
				 lower_32_bits(cpu_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_UPPER_BASE,
				 upper_32_bits(cpu_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_LOWER_LIMIT,
				 lower_32_bits(limit_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_UPPER_LIMIT,
				 upper_32_bits(limit_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_LOWER_TARGET,
				 lower_32_bits(pci_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_UPPER_TARGET,
				 upper_32_bits(pci_addr));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL1,
				 type | PCIE_ATU_FUNC_NUM(func_no));
	dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL2,
				 PCIE_ATU_ENABLE);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = dw_pcie6_readl_ob_unroll(pci, index,
					      PCIE_ATU_UNR_REGION_CTRL2);
		if (val & PCIE_ATU_ENABLE)
			return;

		mdelay(LINK_WAIT_IATU);
	}
	dev_err(pci->dev, "Outbound iATU is not being enabled\n");
}

static void __dw_pcie6_prog_outbound_atu(struct dw_pcie6 *pci, u8 func_no,
					int index, int type, u64 cpu_addr,
					u64 pci_addr, u32 size)
{
	u32 retries, val;

	if (pci->ops->cpu_addr_fixup)
		cpu_addr = pci->ops->cpu_addr_fixup(pci, cpu_addr);

	if (pci->iatu_unroll_enabled) {
		dw_pcie6_prog_outbound_atu_unroll(pci, func_no, index, type,
						 cpu_addr, pci_addr, size);
		return;
	}

	dw_pcie6_writel_dbi(pci, PCIE_ATU_VIEWPORT,
			   PCIE_ATU_REGION_OUTBOUND | index);
	dw_pcie6_writel_dbi(pci, PCIE_ATU_LOWER_BASE,
			   lower_32_bits(cpu_addr));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_UPPER_BASE,
			   upper_32_bits(cpu_addr));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_LIMIT,
			   lower_32_bits(cpu_addr + size - 1));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_LOWER_TARGET,
			   lower_32_bits(pci_addr));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_UPPER_TARGET,
			   upper_32_bits(pci_addr));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_CR1, type |
			   PCIE_ATU_FUNC_NUM(func_no));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_CR2, PCIE_ATU_ENABLE);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = dw_pcie6_readl_dbi(pci, PCIE_ATU_CR2);
		if (val & PCIE_ATU_ENABLE)
			return;

		mdelay(LINK_WAIT_IATU);
	}
	dev_err(pci->dev, "Outbound iATU is not being enabled\n");
}

void dw_pcie6_prog_outbound_atu(struct dw_pcie6 *pci, int index, int type,
			       u64 cpu_addr, u64 pci_addr, u32 size)
{
	__dw_pcie6_prog_outbound_atu(pci, 0, index, type,
				    cpu_addr, pci_addr, size);
}

void dw_pcie6_prog_ep_outbound_atu(struct dw_pcie6 *pci, u8 func_no, int index,
				  int type, u64 cpu_addr, u64 pci_addr,
				  u32 size)
{
	__dw_pcie6_prog_outbound_atu(pci, func_no, index, type,
				    cpu_addr, pci_addr, size);
}

static u32 dw_pcie6_readl_ib_unroll(struct dw_pcie6 *pci, u32 index, u32 reg)
{
	u32 offset = PCIE_GET_ATU_INB_UNR_REG_OFFSET(index);

	return dw_pcie6_readl_atu(pci, offset + reg);
}

static void dw_pcie6_writel_ib_unroll(struct dw_pcie6 *pci, u32 index, u32 reg,
				     u32 val)
{
	u32 offset = PCIE_GET_ATU_INB_UNR_REG_OFFSET(index);

	dw_pcie6_writel_atu(pci, offset + reg, val);
}

static int dw_pcie6_prog_inbound_atu_unroll(struct dw_pcie6 *pci, u8 func_no,
					   int index, int bar, u64 cpu_addr,
					   enum dw_pcie6_as_type as_type)
{
	int type;
	u32 retries, val;

	dw_pcie6_writel_ib_unroll(pci, index, PCIE_ATU_UNR_LOWER_TARGET,
				 lower_32_bits(cpu_addr));
	dw_pcie6_writel_ib_unroll(pci, index, PCIE_ATU_UNR_UPPER_TARGET,
				 upper_32_bits(cpu_addr));

	switch (as_type) {
	case DW_PCIE_AS_MEM:
		type = PCIE_ATU_TYPE_MEM;
		break;
	case DW_PCIE_AS_IO:
		type = PCIE_ATU_TYPE_IO;
		break;
	default:
		return -EINVAL;
	}

	dw_pcie6_writel_ib_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL1, type |
				 PCIE_ATU_FUNC_NUM(func_no));
	dw_pcie6_writel_ib_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL2,
				 PCIE_ATU_FUNC_NUM_MATCH_EN |
				 PCIE_ATU_ENABLE |
				 PCIE_ATU_BAR_MODE_ENABLE | (bar << 8));

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = dw_pcie6_readl_ib_unroll(pci, index,
					      PCIE_ATU_UNR_REGION_CTRL2);
		if (val & PCIE_ATU_ENABLE)
			return 0;

		mdelay(LINK_WAIT_IATU);
	}
	dev_err(pci->dev, "Inbound iATU is not being enabled\n");

	return -EBUSY;
}

int dw_pcie6_prog_inbound_atu(struct dw_pcie6 *pci, u8 func_no, int index,
			     int bar, u64 cpu_addr,
			     enum dw_pcie6_as_type as_type)
{
	int type;
	u32 retries, val;

	if (pci->iatu_unroll_enabled)
		return dw_pcie6_prog_inbound_atu_unroll(pci, func_no, index, bar,
						       cpu_addr, as_type);

	dw_pcie6_writel_dbi(pci, PCIE_ATU_VIEWPORT, PCIE_ATU_REGION_INBOUND |
			   index);
	dw_pcie6_writel_dbi(pci, PCIE_ATU_LOWER_TARGET, lower_32_bits(cpu_addr));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_UPPER_TARGET, upper_32_bits(cpu_addr));

	switch (as_type) {
	case DW_PCIE_AS_MEM:
		type = PCIE_ATU_TYPE_MEM;
		break;
	case DW_PCIE_AS_IO:
		type = PCIE_ATU_TYPE_IO;
		break;
	default:
		return -EINVAL;
	}

	dw_pcie6_writel_dbi(pci, PCIE_ATU_CR1, type |
			   PCIE_ATU_FUNC_NUM(func_no));
	dw_pcie6_writel_dbi(pci, PCIE_ATU_CR2, PCIE_ATU_ENABLE |
			   PCIE_ATU_FUNC_NUM_MATCH_EN |
			   PCIE_ATU_BAR_MODE_ENABLE | (bar << 8));

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = dw_pcie6_readl_dbi(pci, PCIE_ATU_CR2);
		if (val & PCIE_ATU_ENABLE)
			return 0;

		mdelay(LINK_WAIT_IATU);
	}
	dev_err(pci->dev, "Inbound iATU is not being enabled\n");

	return -EBUSY;
}

void dw_pcie6_disable_atu(struct dw_pcie6 *pci, int index,
			 enum dw_pcie6_region_type type)
{
	u32 region;

	switch (type) {
	case DW_PCIE_REGION_INBOUND:
		region = PCIE_ATU_REGION_INBOUND;
		break;
	case DW_PCIE_REGION_OUTBOUND:
		region = PCIE_ATU_REGION_OUTBOUND;
		break;
	default:
		return;
	}

	if (pci->iatu_unroll_enabled) {
		if (region == PCIE_ATU_REGION_INBOUND) {
			dw_pcie6_writel_ib_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL2,
						 ~(u32)PCIE_ATU_ENABLE);
		} else {
			dw_pcie6_writel_ob_unroll(pci, index, PCIE_ATU_UNR_REGION_CTRL2,
						 ~(u32)PCIE_ATU_ENABLE);
		}
	} else {
		dw_pcie6_writel_dbi(pci, PCIE_ATU_VIEWPORT, region | index);
		dw_pcie6_writel_dbi(pci, PCIE_ATU_CR2, ~(u32)PCIE_ATU_ENABLE);
	}
}

int dw_pcie6_wait_for_link(struct dw_pcie6 *pci)
{
	int retries;

	/* Check if the link is up or not */
	for (retries = 0; retries < LINK_WAIT_MAX_RETRIES; retries++) {
		if (dw_pcie6_link_up(pci)) {
			dev_info(pci->dev, "Link up\n");
			return 0;
		}
		usleep_range(LINK_WAIT_USLEEP_MIN, LINK_WAIT_USLEEP_MAX);
	}

	dev_info(pci->dev, "Phy link never came up\n");

	return -ETIMEDOUT;
}

int dw_pcie6_link_up(struct dw_pcie6 *pci)
{
	u32 val;

	if (pci->ops->link_up)
		return pci->ops->link_up(pci);

	val = readl(pci->dbi_base + PCIE_PORT_DEBUG1);
	return ((val & PCIE_PORT_DEBUG1_LINK_UP) &&
		(!(val & PCIE_PORT_DEBUG1_LINK_IN_TRAINING)));
}

void dw_pcie6_upconfig_setup(struct dw_pcie6 *pci)
{
	u32 val;

	val = dw_pcie6_readl_dbi(pci, PCIE_PORT_MULTI_LANE_CTRL);
	val |= PORT_MLTI_UPCFG_SUPPORT;
	dw_pcie6_writel_dbi(pci, PCIE_PORT_MULTI_LANE_CTRL, val);
}

static void dw_pcie6_link_set_max_speed(struct dw_pcie6 *pci, u32 link_gen)
{
	u32 cap, ctrl2, link_speed;
	u8 offset = dw_pcie6_find_capability(pci, PCI_CAP_ID_EXP);

	cap = dw_pcie6_readl_dbi(pci, offset + PCI_EXP_LNKCAP);
	ctrl2 = dw_pcie6_readl_dbi(pci, offset + PCI_EXP_LNKCTL2);
	ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;

	switch (pcie_link_speed[link_gen]) {
	case PCIE_SPEED_2_5GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_2_5GT;
		break;
	case PCIE_SPEED_5_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_5_0GT;
		break;
	case PCIE_SPEED_8_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_8_0GT;
		break;
	case PCIE_SPEED_16_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_16_0GT;
		break;
	default:
		/* Use hardware capability */
		link_speed = FIELD_GET(PCI_EXP_LNKCAP_SLS, cap);
		ctrl2 &= ~PCI_EXP_LNKCTL2_HASD;
		break;
	}

	dw_pcie6_writel_dbi(pci, offset + PCI_EXP_LNKCTL2, ctrl2 | link_speed);

	cap &= ~((u32)PCI_EXP_LNKCAP_SLS);
	dw_pcie6_writel_dbi(pci, offset + PCI_EXP_LNKCAP, cap | link_speed);

}

static u8 dw_pcie6_iatu_unroll_enabled(struct dw_pcie6 *pci)
{
	u32 val;

	val = dw_pcie6_readl_dbi(pci, PCIE_ATU_VIEWPORT);
	if (val == 0xffffffff)
		return 1;

	return 0;
}

void dw_pcie6_setup(struct dw_pcie6 *pci)
{
	u32 val;
	struct device *dev = pci->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);

	if (pci->version >= 0x480A || (!pci->version &&
				       dw_pcie6_iatu_unroll_enabled(pci))) {
		pci->iatu_unroll_enabled = true;
		if (!pci->atu_base)
			pci->atu_base =
			    devm_platform_ioremap_resource_byname(pdev, "atu");
		if (IS_ERR(pci->atu_base))
			pci->atu_base = pci->dbi_base + DEFAULT_DBI_ATU_OFFSET;
	}
	dev_dbg(pci->dev, "iATU unroll: %s\n", pci->iatu_unroll_enabled ?
		"enabled" : "disabled");

	if (pci->link_gen > 0)
		dw_pcie6_link_set_max_speed(pci, pci->link_gen);

	/* Configure Gen1 N_FTS */
	if (pci->n_fts[0]) {
		val = dw_pcie6_readl_dbi(pci, PCIE_PORT_AFR);
		val &= ~(PORT_AFR_N_FTS_MASK | PORT_AFR_CC_N_FTS_MASK);
		val |= PORT_AFR_N_FTS(pci->n_fts[0]);
		val |= PORT_AFR_CC_N_FTS(pci->n_fts[0]);
		dw_pcie6_writel_dbi(pci, PCIE_PORT_AFR, val);
	}

	/* Configure Gen2+ N_FTS */
	if (pci->n_fts[1]) {
		val = dw_pcie6_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
		val &= ~PORT_LOGIC_N_FTS_MASK;
		val |= pci->n_fts[pci->link_gen - 1];
		dw_pcie6_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);
	}

	val = dw_pcie6_readl_dbi(pci, PCIE_PORT_LINK_CONTROL);
	val &= ~PORT_LINK_FAST_LINK_MODE;
	val |= PORT_LINK_DLL_LINK_EN;
	dw_pcie6_writel_dbi(pci, PCIE_PORT_LINK_CONTROL, val);

	if (of_property_read_bool(np, "snps,enable-cdm-check")) {
		val = dw_pcie6_readl_dbi(pci, PCIE_PL_CHK_REG_CONTROL_STATUS);
		val |= PCIE_PL_CHK_REG_CHK_REG_CONTINUOUS |
		       PCIE_PL_CHK_REG_CHK_REG_START;
		dw_pcie6_writel_dbi(pci, PCIE_PL_CHK_REG_CONTROL_STATUS, val);
	}

	of_property_read_u32(np, "num-lanes", &pci->num_lanes);
	if (!pci->num_lanes) {
		dev_dbg(pci->dev, "Using h/w default number of lanes\n");
		return;
	}

	/* Set the number of lanes */
	val &= ~PORT_LINK_FAST_LINK_MODE;
	val &= ~PORT_LINK_MODE_MASK;
	switch (pci->num_lanes) {
	case 1:
		val |= PORT_LINK_MODE_1_LANES;
		break;
	case 2:
		val |= PORT_LINK_MODE_2_LANES;
		break;
	case 4:
		val |= PORT_LINK_MODE_4_LANES;
		break;
	case 8:
		val |= PORT_LINK_MODE_8_LANES;
		break;
	default:
		dev_err(pci->dev, "num-lanes %u: invalid value\n", pci->num_lanes);
		return;
	}
	dw_pcie6_writel_dbi(pci, PCIE_PORT_LINK_CONTROL, val);

	/* Set link width speed control register */
	val = dw_pcie6_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
	val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
	switch (pci->num_lanes) {
	case 1:
		val |= PORT_LOGIC_LINK_WIDTH_1_LANES;
		break;
	case 2:
		val |= PORT_LOGIC_LINK_WIDTH_2_LANES;
		break;
	case 4:
		val |= PORT_LOGIC_LINK_WIDTH_4_LANES;
		break;
	case 8:
		val |= PORT_LOGIC_LINK_WIDTH_8_LANES;
		break;
	}
	dw_pcie6_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);
}

/*** PCIe Designware Host ***/

static struct pci_ops dw_pcie6_bridge_ops;
static struct pci_ops dw_child_pcie_ops;

static void dw_msi_ack_irq(struct irq_data *d)
{
	irq_chip_ack_parent(d);
}

static void dw_msi_mask_irq(struct irq_data *d)
{
	pci_msi_mask_irq(d);
	irq_chip_mask_parent(d);
}

static void dw_msi_unmask_irq(struct irq_data *d)
{
	pci_msi_unmask_irq(d);
	irq_chip_unmask_parent(d);
}

static struct irq_chip dw_pcie6_msi_irq_chip = {
	.name = "PCI-MSI",
	.irq_ack = dw_msi_ack_irq,
	.irq_mask = dw_msi_mask_irq,
	.irq_unmask = dw_msi_unmask_irq,
};

static struct msi_domain_info dw_pcie6_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX | MSI_FLAG_MULTI_PCI_MSI),
	.chip	= &dw_pcie6_msi_irq_chip,
};

/* MSI int handler */
irqreturn_t dw_pcie6_handle_msi_irq(struct pcie_port *pp)
{
	int i, pos, irq;
	unsigned long val;
	u32 status, num_ctrls;
	irqreturn_t ret = IRQ_NONE;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (i = 0; i < num_ctrls; i++) {
		status = dw_pcie6_readl_dbi(pci, PCIE_MSI_INTR0_STATUS +
					   (i * MSI_REG_CTRL_BLOCK_SIZE));
		if (!status)
			continue;

		ret = IRQ_HANDLED;
		val = status;
		pos = 0;
		while ((pos = find_next_bit(&val, MAX_MSI_IRQS_PER_CTRL,
					    pos)) != MAX_MSI_IRQS_PER_CTRL) {
			irq = irq_find_mapping(pp->irq_domain,
					       (i * MAX_MSI_IRQS_PER_CTRL) +
					       pos);
			generic_handle_irq(irq);
			pos++;
		}
	}

	return ret;
}

/* Chained MSI interrupt service routine */
static void dw_chained_msi_isr(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct pcie_port *pp;

	chained_irq_enter(chip, desc);

	pp = irq_desc_get_handler_data(desc);
	dw_pcie6_handle_msi_irq(pp);

	chained_irq_exit(chip, desc);
}

static void dw_pci_setup_msi_msg(struct irq_data *d, struct msi_msg *msg)
{
	struct pcie_port *pp = irq_data_get_irq_chip_data(d);
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	u64 msi_target;

	msi_target = (u64)pp->msi_data;

	msg->address_lo = lower_32_bits(msi_target);
	msg->address_hi = upper_32_bits(msi_target);

	msg->data = d->hwirq;

	dev_dbg(pci->dev, "msi#%d address_hi %#x address_lo %#x\n",
		(int)d->hwirq, msg->address_hi, msg->address_lo);
}

static int dw_pci_msi_set_affinity(struct irq_data *d,
				   const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static void dw_pci_bottom_mask(struct irq_data *d)
{
	struct pcie_port *pp = irq_data_get_irq_chip_data(d);
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	unsigned int res, bit, ctrl;
	unsigned long flags;

	raw_spin_lock_irqsave(&pp->lock, flags);

	ctrl = d->hwirq / MAX_MSI_IRQS_PER_CTRL;
	res = ctrl * MSI_REG_CTRL_BLOCK_SIZE;
	bit = d->hwirq % MAX_MSI_IRQS_PER_CTRL;

	pp->irq_mask[ctrl] |= BIT(bit);
	dw_pcie6_writel_dbi(pci, PCIE_MSI_INTR0_MASK + res, pp->irq_mask[ctrl]);

	raw_spin_unlock_irqrestore(&pp->lock, flags);
}

static void dw_pci_bottom_unmask(struct irq_data *d)
{
	struct pcie_port *pp = irq_data_get_irq_chip_data(d);
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	unsigned int res, bit, ctrl;
	unsigned long flags;

	raw_spin_lock_irqsave(&pp->lock, flags);

	ctrl = d->hwirq / MAX_MSI_IRQS_PER_CTRL;
	res = ctrl * MSI_REG_CTRL_BLOCK_SIZE;
	bit = d->hwirq % MAX_MSI_IRQS_PER_CTRL;

	pp->irq_mask[ctrl] &= ~BIT(bit);
	dw_pcie6_writel_dbi(pci, PCIE_MSI_INTR0_MASK + res, pp->irq_mask[ctrl]);

	raw_spin_unlock_irqrestore(&pp->lock, flags);
}

static void dw_pci_bottom_ack(struct irq_data *d)
{
	struct pcie_port *pp  = irq_data_get_irq_chip_data(d);
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	unsigned int res, bit, ctrl;

	ctrl = d->hwirq / MAX_MSI_IRQS_PER_CTRL;
	res = ctrl * MSI_REG_CTRL_BLOCK_SIZE;
	bit = d->hwirq % MAX_MSI_IRQS_PER_CTRL;

	dw_pcie6_writel_dbi(pci, PCIE_MSI_INTR0_STATUS + res, BIT(bit));
}

static struct irq_chip dw_pci_msi_bottom_irq_chip = {
	.name = "DWPCI-MSI",
	.irq_ack = dw_pci_bottom_ack,
	.irq_compose_msi_msg = dw_pci_setup_msi_msg,
	.irq_set_affinity = dw_pci_msi_set_affinity,
	.irq_mask = dw_pci_bottom_mask,
	.irq_unmask = dw_pci_bottom_unmask,
};

static int dw_pcie6_irq_domain_alloc(struct irq_domain *domain,
				    unsigned int virq, unsigned int nr_irqs,
				    void *args)
{
	struct pcie_port *pp = domain->host_data;
	unsigned long flags;
	u32 i;
	int bit;

	raw_spin_lock_irqsave(&pp->lock, flags);

	bit = bitmap_find_free_region(pp->msi_irq_in_use, pp->num_vectors,
				      order_base_2(nr_irqs));

	raw_spin_unlock_irqrestore(&pp->lock, flags);

	if (bit < 0)
		return -ENOSPC;

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_info(domain, virq + i, bit + i,
				    pp->msi_irq_chip,
				    pp, handle_edge_irq,
				    NULL, NULL);

	return 0;
}

static void dw_pcie6_irq_domain_free(struct irq_domain *domain,
				    unsigned int virq, unsigned int nr_irqs)
{
	struct irq_data *d = irq_domain_get_irq_data(domain, virq);
	struct pcie_port *pp = domain->host_data;
	unsigned long flags;

	raw_spin_lock_irqsave(&pp->lock, flags);

	bitmap_release_region(pp->msi_irq_in_use, d->hwirq,
			      order_base_2(nr_irqs));

	raw_spin_unlock_irqrestore(&pp->lock, flags);
}

static const struct irq_domain_ops dw_pcie6_msi_domain_ops = {
	.alloc	= dw_pcie6_irq_domain_alloc,
	.free	= dw_pcie6_irq_domain_free,
};

int dw_pcie6_allocate_domains(struct pcie_port *pp)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	struct fwnode_handle *fwnode = of_node_to_fwnode(pci->dev->of_node);

	pp->irq_domain = irq_domain_create_linear(fwnode, pp->num_vectors,
					       &dw_pcie6_msi_domain_ops, pp);
	if (!pp->irq_domain) {
		dev_err(pci->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	irq_domain_update_bus_token(pp->irq_domain, DOMAIN_BUS_NEXUS);

	pp->msi_domain = pci_msi_create_irq_domain(fwnode,
						   &dw_pcie6_msi_domain_info,
						   pp->irq_domain);
	if (!pp->msi_domain) {
		dev_err(pci->dev, "Failed to create MSI domain\n");
		irq_domain_remove(pp->irq_domain);
		return -ENOMEM;
	}

	return 0;
}

void dw_pcie6_free_msi(struct pcie_port *pp)
{
	if (pp->msi_irq) {
		irq_set_chained_handler(pp->msi_irq, NULL);
		irq_set_handler_data(pp->msi_irq, NULL);
	}

	irq_domain_remove(pp->msi_domain);
	irq_domain_remove(pp->irq_domain);

	if (pp->msi_data) {
		struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
		struct device *dev = pci->dev;

		dma_unmap_single_attrs(dev, pp->msi_data, sizeof(pp->msi_msg),
				       DMA_FROM_DEVICE, DMA_ATTR_SKIP_CPU_SYNC);
	}
}

void dw_pcie6_msi_init(struct pcie_port *pp)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	u64 msi_target = (u64)pp->msi_data;

	if (!IS_ENABLED(CONFIG_PCI_MSI))
		return;

	/* Program the msi_data */
	dw_pcie6_writel_dbi(pci, PCIE_MSI_ADDR_LO, lower_32_bits(msi_target));
	dw_pcie6_writel_dbi(pci, PCIE_MSI_ADDR_HI, upper_32_bits(msi_target));
}

int dw_pcie6_host_init(struct pcie_port *pp)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);
	struct device *dev = pci->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource_entry *win;
	struct pci_host_bridge *bridge;
	struct resource *cfg_res;
	int ret;

	raw_spin_lock_init(&pci->pp.lock);

	cfg_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "config");
	if (cfg_res) {
		pp->cfg0_size = resource_size(cfg_res);
		pp->cfg0_base = cfg_res->start;
	} else if (!pp->va_cfg0_base) {
		dev_err(dev, "Missing *config* reg space\n");
	}

	bridge = devm_pci_alloc_host_bridge(dev, 0);
	if (!bridge)
		return -ENOMEM;

	pp->bridge = bridge;

	/* Get the I/O and memory ranges from DT */
	resource_list_for_each_entry(win, &bridge->windows) {
		switch (resource_type(win->res)) {
		case IORESOURCE_IO:
			pp->io_size = resource_size(win->res);
			pp->io_bus_addr = win->res->start - win->offset;
			pp->io_base = pci_pio_to_address(win->res->start);
			break;
		case 0:
			dev_err(dev, "Missing *config* reg space\n");
			pp->cfg0_size = resource_size(win->res);
			pp->cfg0_base = win->res->start;
			if (!pci->dbi_base) {
				pci->dbi_base = devm_pci_remap_cfgspace(dev,
								pp->cfg0_base,
								pp->cfg0_size);
				if (!pci->dbi_base) {
					dev_err(dev, "Error with ioremap\n");
					return -ENOMEM;
				}
			}
			break;
		}
	}

	if (!pp->va_cfg0_base) {
		pp->va_cfg0_base = devm_pci_remap_cfgspace(dev,
					pp->cfg0_base, pp->cfg0_size);
		if (!pp->va_cfg0_base) {
			dev_err(dev, "Error with ioremap in function\n");
			return -ENOMEM;
		}
	}

	ret = of_property_read_u32(np, "num-viewport", &pci->num_viewport);
	if (ret)
		pci->num_viewport = 2;

	if (pci->link_gen < 1)
		pci->link_gen = of_pci_get_max_link_speed(np);

	if (pci_msi_enabled()) {
		/*
		 * If a specific SoC driver needs to change the
		 * default number of vectors, it needs to implement
		 * the set_num_vectors callback.
		 */
		if (!pp->ops->set_num_vectors) {
			pp->num_vectors = MSI_DEF_NUM_VECTORS;
		} else {
			pp->ops->set_num_vectors(pp);

			if (pp->num_vectors > MAX_MSI_IRQS ||
			    pp->num_vectors == 0) {
				dev_err(dev,
					"Invalid number of vectors\n");
				return -EINVAL;
			}
		}

		if (!pp->ops->msi_host_init) {
			pp->msi_irq_chip = &dw_pci_msi_bottom_irq_chip;

			ret = dw_pcie6_allocate_domains(pp);
			if (ret)
				return ret;

			if (pp->msi_irq)
				irq_set_chained_handler_and_data(pp->msi_irq,
							    dw_chained_msi_isr,
							    pp);

			pp->msi_data = dma_map_single_attrs(pci->dev, &pp->msi_msg,
						      sizeof(pp->msi_msg),
						      DMA_FROM_DEVICE,
						      DMA_ATTR_SKIP_CPU_SYNC);
			ret = dma_mapping_error(pci->dev, pp->msi_data);
			if (ret) {
				dev_err(pci->dev, "Failed to map MSI data\n");
				pp->msi_data = 0;
				goto err_free_msi;
			}
		} else {
			ret = pp->ops->msi_host_init(pp);
			if (ret < 0)
				return ret;
		}
	}

	/* Set default bus ops */
	bridge->ops = &dw_pcie6_bridge_ops;
	bridge->child_ops = &dw_child_pcie_ops;

	if (pp->ops->host_init) {
		ret = pp->ops->host_init(pp);
		if (ret)
			goto err_free_msi;
	}

	bridge->sysdata = pp;

	ret = pci_host_probe(bridge);
	if (!ret)
		return 0;

err_free_msi:
	if (pci_msi_enabled() && !pp->ops->msi_host_init)
		dw_pcie6_free_msi(pp);
	return ret;
}

void dw_pcie6_host_deinit(struct pcie_port *pp)
{
	pci_stop_root_bus(pp->bridge->bus);
	pci_remove_root_bus(pp->bridge->bus);
	if (pci_msi_enabled() && !pp->ops->msi_host_init)
		dw_pcie6_free_msi(pp);
}

static void __iomem *dw_pcie6_other_conf_map_bus(struct pci_bus *bus,
						unsigned int devfn, int where)
{
	int type;
	u32 busdev;
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	/*
	 * Checking whether the link is up here is a last line of defense
	 * against platforms that forward errors on the system bus as
	 * SError upon PCI configuration transactions issued when the link
	 * is down. This check is racy by definition and does not stop
	 * the system from triggering an SError if the link goes down
	 * after this check is performed.
	 */
	if (!dw_pcie6_link_up(pci))
		return NULL;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));

	if (pci_is_root_bus(bus->parent))
		type = PCIE_ATU_TYPE_CFG0;
	else
		type = PCIE_ATU_TYPE_CFG1;


	dw_pcie6_prog_outbound_atu(pci, PCIE_ATU_REGION_INDEX1,
				  type, pp->cfg0_base,
				  busdev, pp->cfg0_size);

	return pp->va_cfg0_base + where;
}

static int dw_pcie6_rd_other_conf(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 *val)
{
	int ret;
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	ret = pci_generic_config_read(bus, devfn, where, size, val);

	if (!ret && pci->num_viewport <= 2)
		dw_pcie6_prog_outbound_atu(pci, PCIE_ATU_REGION_INDEX1,
					  PCIE_ATU_TYPE_IO, pp->io_base,
					  pp->io_bus_addr, pp->io_size);

	return ret;
}

static int dw_pcie6_wr_other_conf(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	int ret;
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	ret = pci_generic_config_write(bus, devfn, where, size, val);

	if (!ret && pci->num_viewport <= 2)
		dw_pcie6_prog_outbound_atu(pci, PCIE_ATU_REGION_INDEX1,
					  PCIE_ATU_TYPE_IO, pp->io_base,
					  pp->io_bus_addr, pp->io_size);

	return ret;
}

static struct pci_ops dw_child_pcie_ops = {
	.map_bus = dw_pcie6_other_conf_map_bus,
	.read = dw_pcie6_rd_other_conf,
	.write = dw_pcie6_wr_other_conf,
};

void __iomem *dw_pcie6_own_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	if (PCI_SLOT(devfn) > 0)
		return NULL;

	return pci->dbi_base + where;
}

static struct pci_ops dw_pcie6_bridge_ops = {
	.map_bus = dw_pcie6_own_conf_map_bus,
	.read = pci_generic_config_read,
	.write = pci_generic_config_write,
};

void dw_pcie6_setup_rc(struct pcie_port *pp)
{
	u32 val, ctrl, num_ctrls;
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	/*
	 * Enable DBI read-only registers for writing/updating configuration.
	 * Write permission gets disabled towards the end of this function.
	 */
	dw_pcie6_dbi_ro_wr_en(pci);

	dw_pcie6_setup(pci);

	if (pci_msi_enabled() && !pp->ops->msi_host_init) {
		num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

		/* Initialize IRQ Status array */
		for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
			pp->irq_mask[ctrl] = ~0;
			dw_pcie6_writel_dbi(pci, PCIE_MSI_INTR0_MASK +
					    (ctrl * MSI_REG_CTRL_BLOCK_SIZE),
					    pp->irq_mask[ctrl]);
			dw_pcie6_writel_dbi(pci, PCIE_MSI_INTR0_ENABLE +
					    (ctrl * MSI_REG_CTRL_BLOCK_SIZE),
					    ~0);
		}
	}

	/* Setup RC BARs */
	dw_pcie6_writel_dbi(pci, PCI_BASE_ADDRESS_0, 0x00000004);
	dw_pcie6_writel_dbi(pci, PCI_BASE_ADDRESS_1, 0x00000000);

	/* Setup interrupt pins */
	val = dw_pcie6_readl_dbi(pci, PCI_INTERRUPT_LINE);
	val &= 0xffff00ff;
	val |= 0x00000100;
	dw_pcie6_writel_dbi(pci, PCI_INTERRUPT_LINE, val);

	/* Setup bus numbers */
	val = dw_pcie6_readl_dbi(pci, PCI_PRIMARY_BUS);
	val &= 0xff000000;
	val |= 0x00ff0100;
	dw_pcie6_writel_dbi(pci, PCI_PRIMARY_BUS, val);

	/* Setup command register */
	val = dw_pcie6_readl_dbi(pci, PCI_COMMAND);
	val &= 0xffff0000;
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
	dw_pcie6_writel_dbi(pci, PCI_COMMAND, val);

	/*
	 * If the platform provides its own child bus config accesses, it means
	 * the platform uses its own address translation component rather than
	 * ATU, so we should not program the ATU here.
	 */
	if (pp->bridge->child_ops == &dw_child_pcie_ops) {
		struct resource_entry *tmp, *entry = NULL;

		/* Get last memory resource entry */
		resource_list_for_each_entry(tmp, &pp->bridge->windows)
			if (resource_type(tmp->res) == IORESOURCE_MEM)
				entry = tmp;

		dw_pcie6_prog_outbound_atu(pci, PCIE_ATU_REGION_INDEX0,
					  PCIE_ATU_TYPE_MEM, entry->res->start,
					  entry->res->start - entry->offset,
					  resource_size(entry->res));
		if (pci->num_viewport > 2)
			dw_pcie6_prog_outbound_atu(pci, PCIE_ATU_REGION_INDEX2,
						  PCIE_ATU_TYPE_IO, pp->io_base,
						  pp->io_bus_addr, pp->io_size);
	}

	dw_pcie6_writel_dbi(pci, PCI_BASE_ADDRESS_0, 0);

	/* Program correct class for RC */
	dw_pcie6_writew_dbi(pci, PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

	val = dw_pcie6_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
	val |= PORT_LOGIC_SPEED_CHANGE;
	dw_pcie6_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);

	dw_pcie6_dbi_ro_wr_dis(pci);
}

/*** PCIe Designware Endpoint ***/

void dw_pcie6_ep_linkup(struct dw_pcie6_ep *ep)
{
	struct pci_epc *epc = ep->epc;

	pci_epc_linkup(epc);
}

void dw_pcie6_ep_init_notify(struct dw_pcie6_ep *ep)
{
	struct pci_epc *epc = ep->epc;

	pci_epc_init_notify(epc);
}

struct dw_pcie6_ep_func *
dw_pcie6_ep_get_func_from_ep(struct dw_pcie6_ep *ep, u8 func_no)
{
	struct dw_pcie6_ep_func *ep_func;

	list_for_each_entry(ep_func, &ep->func_list, list) {
		if (ep_func->func_no == func_no)
			return ep_func;
	}

	return NULL;
}

static unsigned int dw_pcie6_ep_func_select(struct dw_pcie6_ep *ep, u8 func_no)
{
	unsigned int func_offset = 0;

	if (ep->ops->func_conf_select)
		func_offset = ep->ops->func_conf_select(ep, func_no);

	return func_offset;
}

static void __dw_pcie6_ep_reset_bar(struct dw_pcie6 *pci, u8 func_no,
				   enum pci_barno bar, int flags)
{
	u32 reg;
	unsigned int func_offset = 0;
	struct dw_pcie6_ep *ep = &pci->ep;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = func_offset + PCI_BASE_ADDRESS_0 + (4 * bar);
	dw_pcie6_dbi_ro_wr_en(pci);
	dw_pcie6_writel_dbi2(pci, reg, 0x0);
	dw_pcie6_writel_dbi(pci, reg, 0x0);
	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		dw_pcie6_writel_dbi2(pci, reg + 4, 0x0);
		dw_pcie6_writel_dbi(pci, reg + 4, 0x0);
	}
	dw_pcie6_dbi_ro_wr_dis(pci);
}

void dw_pcie6_ep_reset_bar(struct dw_pcie6 *pci, enum pci_barno bar)
{
	u8 func_no, funcs;

	funcs = pci->ep.epc->max_functions;

	for (func_no = 0; func_no < funcs; func_no++)
		__dw_pcie6_ep_reset_bar(pci, func_no, bar, 0);
}

static u8 __dw_pcie6_ep_find_next_cap(struct dw_pcie6_ep *ep, u8 func_no,
		u8 cap_ptr, u8 cap)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	unsigned int func_offset = 0;
	u8 cap_id, next_cap_ptr;
	u16 reg;

	if (!cap_ptr)
		return 0;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = dw_pcie6_readw_dbi(pci, func_offset + cap_ptr);
	cap_id = (reg & 0x00ff);

	if (cap_id > PCI_CAP_ID_MAX)
		return 0;

	if (cap_id == cap)
		return cap_ptr;

	next_cap_ptr = (reg & 0xff00) >> 8;
	return __dw_pcie6_ep_find_next_cap(ep, func_no, next_cap_ptr, cap);
}

static u8 dw_pcie6_ep_find_capability(struct dw_pcie6_ep *ep, u8 func_no, u8 cap)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	unsigned int func_offset = 0;
	u8 next_cap_ptr;
	u16 reg;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = dw_pcie6_readw_dbi(pci, func_offset + PCI_CAPABILITY_LIST);
	next_cap_ptr = (reg & 0x00ff);

	return __dw_pcie6_ep_find_next_cap(ep, func_no, next_cap_ptr, cap);
}

static int dw_pcie6_ep_write_header(struct pci_epc *epc, u8 func_no,
				   struct pci_epf_header *hdr)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	unsigned int func_offset = 0;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	dw_pcie6_dbi_ro_wr_en(pci);
	dw_pcie6_writew_dbi(pci, func_offset + PCI_VENDOR_ID, hdr->vendorid);
	dw_pcie6_writew_dbi(pci, func_offset + PCI_DEVICE_ID, hdr->deviceid);
	dw_pcie6_writeb_dbi(pci, func_offset + PCI_REVISION_ID, hdr->revid);
	dw_pcie6_writeb_dbi(pci, func_offset + PCI_CLASS_PROG, hdr->progif_code);
	dw_pcie6_writew_dbi(pci, func_offset + PCI_CLASS_DEVICE,
			   hdr->subclass_code | hdr->baseclass_code << 8);
	dw_pcie6_writeb_dbi(pci, func_offset + PCI_CACHE_LINE_SIZE,
			   hdr->cache_line_size);
	dw_pcie6_writew_dbi(pci, func_offset + PCI_SUBSYSTEM_VENDOR_ID,
			   hdr->subsys_vendor_id);
	dw_pcie6_writew_dbi(pci, func_offset + PCI_SUBSYSTEM_ID, hdr->subsys_id);
	dw_pcie6_writeb_dbi(pci, func_offset + PCI_INTERRUPT_PIN,
			   hdr->interrupt_pin);
	dw_pcie6_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie6_ep_inbound_atu(struct dw_pcie6_ep *ep, u8 func_no,
				  enum pci_barno bar, dma_addr_t cpu_addr,
				  enum dw_pcie6_as_type as_type)
{
	int ret;
	u32 free_win;
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	free_win = find_first_zero_bit(ep->ib_window_map, ep->num_ib_windows);
	if (free_win >= ep->num_ib_windows) {
		dev_err(pci->dev, "No free inbound window\n");
		return -EINVAL;
	}

	ret = dw_pcie6_prog_inbound_atu(pci, func_no, free_win, bar, cpu_addr,
				       as_type);
	if (ret < 0) {
		dev_err(pci->dev, "Failed to program IB window\n");
		return ret;
	}

	ep->bar_to_atu[bar] = free_win;
	set_bit(free_win, ep->ib_window_map);

	return 0;
}

static int dw_pcie6_ep_outbound_atu(struct dw_pcie6_ep *ep, u8 func_no,
				   phys_addr_t phys_addr,
				   u64 pci_addr, size_t size)
{
	u32 free_win;
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	free_win = find_first_zero_bit(ep->ob_window_map, ep->num_ob_windows);
	if (free_win >= ep->num_ob_windows) {
		dev_err(pci->dev, "No free outbound window\n");
		return -EINVAL;
	}

	dw_pcie6_prog_ep_outbound_atu(pci, func_no, free_win, PCIE_ATU_TYPE_MEM,
				     phys_addr, pci_addr, size);

	set_bit(free_win, ep->ob_window_map);
	ep->outbound_addr[free_win] = phys_addr;

	return 0;
}

static void dw_pcie6_ep_clear_bar(struct pci_epc *epc, u8 func_no,
				 struct pci_epf_bar *epf_bar)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	enum pci_barno bar = epf_bar->barno;
	u32 atu_index = ep->bar_to_atu[bar];

	__dw_pcie6_ep_reset_bar(pci, func_no, bar, epf_bar->flags);

	dw_pcie6_disable_atu(pci, atu_index, DW_PCIE_REGION_INBOUND);
	clear_bit(atu_index, ep->ib_window_map);
	ep->epf_bar[bar] = NULL;
}

static int dw_pcie6_ep_set_bar(struct pci_epc *epc, u8 func_no,
			      struct pci_epf_bar *epf_bar)
{
	int ret;
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	enum pci_barno bar = epf_bar->barno;
	size_t size = epf_bar->size;
	int flags = epf_bar->flags;
	enum dw_pcie6_as_type as_type;
	u32 reg;
	unsigned int func_offset = 0;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = PCI_BASE_ADDRESS_0 + (4 * bar) + func_offset;

	if (!(flags & PCI_BASE_ADDRESS_SPACE))
		as_type = DW_PCIE_AS_MEM;
	else
		as_type = DW_PCIE_AS_IO;

	ret = dw_pcie6_ep_inbound_atu(ep, func_no, bar,
				     epf_bar->phys_addr, as_type);
	if (ret)
		return ret;

	dw_pcie6_dbi_ro_wr_en(pci);

	dw_pcie6_writel_dbi2(pci, reg, lower_32_bits(size - 1));
	dw_pcie6_writel_dbi(pci, reg, flags);

	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		dw_pcie6_writel_dbi2(pci, reg + 4, upper_32_bits(size - 1));
		dw_pcie6_writel_dbi(pci, reg + 4, 0);
	}

	ep->epf_bar[bar] = epf_bar;
	dw_pcie6_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie6_find_index(struct dw_pcie6_ep *ep, phys_addr_t addr,
			      u32 *atu_index)
{
	u32 index;

	for (index = 0; index < ep->num_ob_windows; index++) {
		if (ep->outbound_addr[index] != addr)
			continue;
		*atu_index = index;
		return 0;
	}

	return -EINVAL;
}

static void dw_pcie6_ep_unmap_addr(struct pci_epc *epc, u8 func_no,
				  phys_addr_t addr)
{
	int ret;
	u32 atu_index;
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	ret = dw_pcie6_find_index(ep, addr, &atu_index);
	if (ret < 0)
		return;

	dw_pcie6_disable_atu(pci, atu_index, DW_PCIE_REGION_OUTBOUND);
	clear_bit(atu_index, ep->ob_window_map);
}

static int dw_pcie6_ep_map_addr(struct pci_epc *epc, u8 func_no,
			       phys_addr_t addr,
			       u64 pci_addr, size_t size)
{
	int ret;
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	ret = dw_pcie6_ep_outbound_atu(ep, func_no, addr, pci_addr, size);
	if (ret) {
		dev_err(pci->dev, "Failed to enable address\n");
		return ret;
	}

	return 0;
}

static int dw_pcie6_ep_get_msi(struct pci_epc *epc, u8 func_no)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	u32 val, reg;
	unsigned int func_offset = 0;
	struct dw_pcie6_ep_func *ep_func;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msi_cap)
		return -EINVAL;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = ep_func->msi_cap + func_offset + PCI_MSI_FLAGS;
	val = dw_pcie6_readw_dbi(pci, reg);
	if (!(val & PCI_MSI_FLAGS_ENABLE))
		return -EINVAL;

	val = (val & PCI_MSI_FLAGS_QSIZE) >> 4;

	return val;
}

static int dw_pcie6_ep_set_msi(struct pci_epc *epc, u8 func_no, u8 interrupts)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	u32 val, reg;
	unsigned int func_offset = 0;
	struct dw_pcie6_ep_func *ep_func;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msi_cap)
		return -EINVAL;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = ep_func->msi_cap + func_offset + PCI_MSI_FLAGS;
	val = dw_pcie6_readw_dbi(pci, reg);
	val &= ~PCI_MSI_FLAGS_QMASK;
	val |= (interrupts << 1) & PCI_MSI_FLAGS_QMASK;
	dw_pcie6_dbi_ro_wr_en(pci);
	dw_pcie6_writew_dbi(pci, reg, val);
	dw_pcie6_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie6_ep_get_msix(struct pci_epc *epc, u8 func_no)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	u32 val, reg;
	unsigned int func_offset = 0;
	struct dw_pcie6_ep_func *ep_func;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msix_cap)
		return -EINVAL;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = ep_func->msix_cap + func_offset + PCI_MSIX_FLAGS;
	val = dw_pcie6_readw_dbi(pci, reg);
	if (!(val & PCI_MSIX_FLAGS_ENABLE))
		return -EINVAL;

	val &= PCI_MSIX_FLAGS_QSIZE;

	return val;
}

static int dw_pcie6_ep_set_msix(struct pci_epc *epc, u8 func_no, u16 interrupts,
			       enum pci_barno bir, u32 offset)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	u32 val, reg;
	unsigned int func_offset = 0;
	struct dw_pcie6_ep_func *ep_func;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msix_cap)
		return -EINVAL;

	dw_pcie6_dbi_ro_wr_en(pci);

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = ep_func->msix_cap + func_offset + PCI_MSIX_FLAGS;
	val = dw_pcie6_readw_dbi(pci, reg);
	val &= ~PCI_MSIX_FLAGS_QSIZE;
	val |= interrupts;
	dw_pcie6_writew_dbi(pci, reg, val);

	reg = ep_func->msix_cap + func_offset + PCI_MSIX_TABLE;
	val = offset | bir;
	dw_pcie6_writel_dbi(pci, reg, val);

	reg = ep_func->msix_cap + func_offset + PCI_MSIX_PBA;
	val = (offset + (interrupts * PCI_MSIX_ENTRY_SIZE)) | bir;
	dw_pcie6_writel_dbi(pci, reg, val);

	dw_pcie6_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie6_ep_raise_irq(struct pci_epc *epc, u8 func_no,
				enum pci_epc_irq_type type, u16 interrupt_num)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);

	if (!ep->ops->raise_irq)
		return -EINVAL;

	return ep->ops->raise_irq(ep, func_no, type, interrupt_num);
}

static void dw_pcie6_ep_stop(struct pci_epc *epc)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	if (!pci->ops->stop_link)
		return;

	pci->ops->stop_link(pci);
}

static int dw_pcie6_ep_start(struct pci_epc *epc)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	if (!pci->ops->start_link)
		return -EINVAL;

	return pci->ops->start_link(pci);
}

static const struct pci_epc_features*
dw_pcie6_ep_get_features(struct pci_epc *epc, u8 func_no)
{
	struct dw_pcie6_ep *ep = epc_get_drvdata(epc);

	if (!ep->ops->get_features)
		return NULL;

	return ep->ops->get_features(ep);
}

static const struct pci_epc_ops epc_ops = {
	.write_header		= dw_pcie6_ep_write_header,
	.set_bar		= dw_pcie6_ep_set_bar,
	.clear_bar		= dw_pcie6_ep_clear_bar,
	.map_addr		= dw_pcie6_ep_map_addr,
	.unmap_addr		= dw_pcie6_ep_unmap_addr,
	.set_msi		= dw_pcie6_ep_set_msi,
	.get_msi		= dw_pcie6_ep_get_msi,
	.set_msix		= dw_pcie6_ep_set_msix,
	.get_msix		= dw_pcie6_ep_get_msix,
	.raise_irq		= dw_pcie6_ep_raise_irq,
	.start			= dw_pcie6_ep_start,
	.stop			= dw_pcie6_ep_stop,
	.get_features		= dw_pcie6_ep_get_features,
};

int dw_pcie6_ep_raise_legacy_irq(struct dw_pcie6_ep *ep, u8 func_no)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	struct device *dev = pci->dev;

	dev_err(dev, "EP cannot trigger legacy IRQs\n");

	return -EINVAL;
}

int dw_pcie6_ep_raise_msi_irq(struct dw_pcie6_ep *ep, u8 func_no,
			     u8 interrupt_num)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	struct dw_pcie6_ep_func *ep_func;
	struct pci_epc *epc = ep->epc;
	unsigned int aligned_offset;
	unsigned int func_offset = 0;
	u16 msg_ctrl, msg_data;
	u32 msg_addr_lower, msg_addr_upper, reg;
	u64 msg_addr;
	bool has_upper;
	int ret;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msi_cap)
		return -EINVAL;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	/* Raise MSI per the PCI Local Bus Specification Revision 3.0, 6.8.1. */
	reg = ep_func->msi_cap + func_offset + PCI_MSI_FLAGS;
	msg_ctrl = dw_pcie6_readw_dbi(pci, reg);
	has_upper = !!(msg_ctrl & PCI_MSI_FLAGS_64BIT);
	reg = ep_func->msi_cap + func_offset + PCI_MSI_ADDRESS_LO;
	msg_addr_lower = dw_pcie6_readl_dbi(pci, reg);
	if (has_upper) {
		reg = ep_func->msi_cap + func_offset + PCI_MSI_ADDRESS_HI;
		msg_addr_upper = dw_pcie6_readl_dbi(pci, reg);
		reg = ep_func->msi_cap + func_offset + PCI_MSI_DATA_64;
		msg_data = dw_pcie6_readw_dbi(pci, reg);
	} else {
		msg_addr_upper = 0;
		reg = ep_func->msi_cap + func_offset + PCI_MSI_DATA_32;
		msg_data = dw_pcie6_readw_dbi(pci, reg);
	}
	aligned_offset = msg_addr_lower & (epc->mem->window.page_size - 1);
	msg_addr = ((u64)msg_addr_upper) << 32 |
			(msg_addr_lower & ~aligned_offset);
	ret = dw_pcie6_ep_map_addr(epc, func_no, ep->msi_mem_phys, msg_addr,
				  epc->mem->window.page_size);
	if (ret)
		return ret;

	writel(msg_data | (interrupt_num - 1), ep->msi_mem + aligned_offset);

	dw_pcie6_ep_unmap_addr(epc, func_no, ep->msi_mem_phys);

	return 0;
}

int dw_pcie6_ep_raise_msix_irq_doorbell(struct dw_pcie6_ep *ep, u8 func_no,
				       u16 interrupt_num)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	struct dw_pcie6_ep_func *ep_func;
	u32 msg_data;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msix_cap)
		return -EINVAL;

	msg_data = (func_no << PCIE_MSIX_DOORBELL_PF_SHIFT) |
		   (interrupt_num - 1);

	dw_pcie6_writel_dbi(pci, PCIE_MSIX_DOORBELL, msg_data);

	return 0;
}

int dw_pcie6_ep_raise_msix_irq(struct dw_pcie6_ep *ep, u8 func_no,
			      u16 interrupt_num)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	struct dw_pcie6_ep_func *ep_func;
	struct pci_epf_msix_tbl *msix_tbl;
	struct pci_epc *epc = ep->epc;
	unsigned int func_offset = 0;
	u32 reg, msg_data, vec_ctrl;
	unsigned int aligned_offset;
	u32 tbl_offset;
	u64 msg_addr;
	int ret;
	u8 bir;

	ep_func = dw_pcie6_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msix_cap)
		return -EINVAL;

	func_offset = dw_pcie6_ep_func_select(ep, func_no);

	reg = ep_func->msix_cap + func_offset + PCI_MSIX_TABLE;
	tbl_offset = dw_pcie6_readl_dbi(pci, reg);
	bir = (tbl_offset & PCI_MSIX_TABLE_BIR);
	tbl_offset &= PCI_MSIX_TABLE_OFFSET;

	msix_tbl = ep->epf_bar[bir]->addr + tbl_offset;
	msg_addr = msix_tbl[(interrupt_num - 1)].msg_addr;
	msg_data = msix_tbl[(interrupt_num - 1)].msg_data;
	vec_ctrl = msix_tbl[(interrupt_num - 1)].vector_ctrl;

	if (vec_ctrl & PCI_MSIX_ENTRY_CTRL_MASKBIT) {
		dev_dbg(pci->dev, "MSI-X entry ctrl set\n");
		return -EPERM;
	}

	aligned_offset = msg_addr & (epc->mem->window.page_size - 1);
	ret = dw_pcie6_ep_map_addr(epc, func_no, ep->msi_mem_phys,  msg_addr,
				  epc->mem->window.page_size);
	if (ret)
		return ret;

	writel(msg_data, ep->msi_mem + aligned_offset);

	dw_pcie6_ep_unmap_addr(epc, func_no, ep->msi_mem_phys);

	return 0;
}

void dw_pcie6_ep_exit(struct dw_pcie6_ep *ep)
{
	struct pci_epc *epc = ep->epc;

	pci_epc_mem_free_addr(epc, ep->msi_mem_phys, ep->msi_mem,
			      epc->mem->window.page_size);

	pci_epc_mem_exit(epc);
}

static unsigned int dw_pcie6_ep_find_ext_capability(struct dw_pcie6 *pci, int cap)
{
	u32 header;
	int pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		header = dw_pcie6_readl_dbi(pci, pos);
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (!pos)
			break;
	}

	return 0;
}

int dw_pcie6_ep_init_complete(struct dw_pcie6_ep *ep)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	unsigned int offset;
	unsigned int nbars;
	u8 hdr_type;
	u32 reg;
	int i;

	hdr_type = dw_pcie6_readb_dbi(pci, PCI_HEADER_TYPE) &
		   PCI_HEADER_TYPE_MASK;
	if (hdr_type != PCI_HEADER_TYPE_NORMAL) {
		dev_err(pci->dev,
			"PCIe controller is not set to EP mode (hdr_type:0x%x)!\n",
			hdr_type);
		return -EIO;
	}

	offset = dw_pcie6_ep_find_ext_capability(pci, PCI_EXT_CAP_ID_REBAR);

	dw_pcie6_dbi_ro_wr_en(pci);

	if (offset) {
		reg = dw_pcie6_readl_dbi(pci, offset + PCI_REBAR_CTRL);
		nbars = (reg & PCI_REBAR_CTRL_NBAR_MASK) >>
			PCI_REBAR_CTRL_NBAR_SHIFT;

		for (i = 0; i < nbars; i++, offset += PCI_REBAR_CTRL)
			dw_pcie6_writel_dbi(pci, offset + PCI_REBAR_CAP, 0x0);
	}

	dw_pcie6_setup(pci);
	dw_pcie6_dbi_ro_wr_dis(pci);

	return 0;
}

int dw_pcie6_ep_init(struct dw_pcie6_ep *ep)
{
	int ret;
	void *addr;
	u8 func_no;
	struct pci_epc *epc;
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	struct device *dev = pci->dev;
	struct device_node *np = dev->of_node;
	const struct pci_epc_features *epc_features;
	struct dw_pcie6_ep_func *ep_func;

	INIT_LIST_HEAD(&ep->func_list);

	if (!pci->dbi_base || !pci->dbi_base2) {
		dev_err(dev, "dbi_base/dbi_base2 is not populated\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num-ib-windows", &ep->num_ib_windows);
	if (ret < 0) {
		dev_err(dev, "Unable to read *num-ib-windows* property\n");
		return ret;
	}
	if (ep->num_ib_windows > MAX_IATU_IN) {
		dev_err(dev, "Invalid *num-ib-windows*\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num-ob-windows", &ep->num_ob_windows);
	if (ret < 0) {
		dev_err(dev, "Unable to read *num-ob-windows* property\n");
		return ret;
	}
	if (ep->num_ob_windows > MAX_IATU_OUT) {
		dev_err(dev, "Invalid *num-ob-windows*\n");
		return -EINVAL;
	}

	ep->ib_window_map = devm_kcalloc(dev,
					 BITS_TO_LONGS(ep->num_ib_windows),
					 sizeof(long),
					 GFP_KERNEL);
	if (!ep->ib_window_map)
		return -ENOMEM;

	ep->ob_window_map = devm_kcalloc(dev,
					 BITS_TO_LONGS(ep->num_ob_windows),
					 sizeof(long),
					 GFP_KERNEL);
	if (!ep->ob_window_map)
		return -ENOMEM;

	addr = devm_kcalloc(dev, ep->num_ob_windows, sizeof(phys_addr_t),
			    GFP_KERNEL);
	if (!addr)
		return -ENOMEM;
	ep->outbound_addr = addr;

	if (pci->link_gen < 1)
		pci->link_gen = of_pci_get_max_link_speed(np);

	epc = devm_pci_epc_create(dev, &epc_ops);
	if (IS_ERR(epc)) {
		dev_err(dev, "Failed to create epc device\n");
		return PTR_ERR(epc);
	}

	ep->epc = epc;
	epc_set_drvdata(epc, ep);

	ret = of_property_read_u8(np, "max-functions", &epc->max_functions);
	if (ret < 0)
		epc->max_functions = 1;

	for (func_no = 0; func_no < epc->max_functions; func_no++) {
		ep_func = devm_kzalloc(dev, sizeof(*ep_func), GFP_KERNEL);
		if (!ep_func)
			return -ENOMEM;

		ep_func->func_no = func_no;
		ep_func->msi_cap = dw_pcie6_ep_find_capability(ep, func_no,
							      PCI_CAP_ID_MSI);
		ep_func->msix_cap = dw_pcie6_ep_find_capability(ep, func_no,
							       PCI_CAP_ID_MSIX);

		list_add_tail(&ep_func->list, &ep->func_list);
	}

	if (ep->ops->ep_init)
		ep->ops->ep_init(ep);

	ret = pci_epc_mem_init(epc, ep->phys_base, ep->addr_size,
			       ep->page_size);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize address space\n");
		return ret;
	}

	ep->msi_mem = pci_epc_mem_alloc_addr(epc, &ep->msi_mem_phys,
					     epc->mem->window.page_size);
	if (!ep->msi_mem) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to reserve memory for MSI/MSI-X\n");
		goto err_exit_epc_mem;
	}

	if (ep->ops->get_features) {
		epc_features = ep->ops->get_features(ep);
		if (epc_features->core_init_notifier)
			return 0;
	}

	ret = dw_pcie6_ep_init_complete(ep);
	if (ret)
		goto err_free_epc_mem;

	return 0;

err_free_epc_mem:
	pci_epc_mem_free_addr(epc, ep->msi_mem_phys, ep->msi_mem,
			      epc->mem->window.page_size);

err_exit_epc_mem:
	pci_epc_mem_exit(epc);

	return ret;
}

/*** PCIe Designware Platform ***/

struct dw_plat_pcie6 {
	struct dw_pcie6			*pci;
	struct regmap			*regmap;
	enum dw_pcie6_device_mode	mode;
};

struct dw_plat_pcie6_of_data {
	enum dw_pcie6_device_mode	mode;
};

static const struct of_device_id dw_plat_pcie6_of_match[];

static int dw_plat_pcie6_host_init(struct pcie_port *pp)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_pp(pp);

	dw_pcie6_setup_rc(pp);
	dw_pcie6_wait_for_link(pci);
	dw_pcie6_msi_init(pp);

	return 0;
}

static void dw_plat_set_num_vectors(struct pcie_port *pp)
{
	pp->num_vectors = MAX_MSI_IRQS;
}

static const struct dw_pcie6_host_ops dw_plat_pcie6_host_ops = {
	.host_init = dw_plat_pcie6_host_init,
	.set_num_vectors = dw_plat_set_num_vectors,
};

static int dw_plat_pcie6_establish_link(struct dw_pcie6 *pci)
{
	return 0;
}

static const struct dw_pcie6_ops dw_pcie6_ops = {
	.start_link = dw_plat_pcie6_establish_link,
};

static void dw_plat_pcie6_ep_init(struct dw_pcie6_ep *ep)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);
	enum pci_barno bar;

	for (bar = 0; bar < PCI_STD_NUM_BARS; bar++)
		dw_pcie6_ep_reset_bar(pci, bar);
}

static int dw_plat_pcie6_ep_raise_irq(struct dw_pcie6_ep *ep, u8 func_no,
				     enum pci_epc_irq_type type,
				     u16 interrupt_num)
{
	struct dw_pcie6 *pci = to_dw_pcie6_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return dw_pcie6_ep_raise_legacy_irq(ep, func_no);
	case PCI_EPC_IRQ_MSI:
		return dw_pcie6_ep_raise_msi_irq(ep, func_no, interrupt_num);
	case PCI_EPC_IRQ_MSIX:
		return dw_pcie6_ep_raise_msix_irq(ep, func_no, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
	}

	return 0;
}

static const struct pci_epc_features dw_plat_pcie6_epc_features = {
	.linkup_notifier = false,
	.msi_capable = true,
	.msix_capable = true,
};

static const struct pci_epc_features*
dw_plat_pcie6_get_features(struct dw_pcie6_ep *ep)
{
	return &dw_plat_pcie6_epc_features;
}

static const struct dw_pcie6_ep_ops pcie_ep_ops = {
	.ep_init = dw_plat_pcie6_ep_init,
	.raise_irq = dw_plat_pcie6_ep_raise_irq,
	.get_features = dw_plat_pcie6_get_features,
};

static int dw_plat_add_pcie_port(struct dw_plat_pcie6 *dw_plat_pcie6,
				 struct platform_device *pdev)
{
	struct dw_pcie6 *pci = dw_plat_pcie6->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = &pdev->dev;
	int ret;

	pp->irq = platform_get_irq(pdev, 1);
	if (pp->irq < 0)
		return pp->irq;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (pp->msi_irq < 0)
			return pp->msi_irq;
	}

	pp->ops = &dw_plat_pcie6_host_ops;

	ret = dw_pcie6_host_init(pp);
	if (ret) {
		dev_err(dev, "Failed to initialize host\n");
		return ret;
	}

	return 0;
}

static int dw_plat_add_pcie_ep(struct dw_plat_pcie6 *dw_plat_pcie6,
			       struct platform_device *pdev)
{
	int ret;
	struct dw_pcie6_ep *ep;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct dw_pcie6 *pci = dw_plat_pcie6->pci;

	ep = &pci->ep;
	ep->ops = &pcie_ep_ops;

	pci->dbi_base2 = devm_platform_ioremap_resource_byname(pdev, "dbi2");
	if (IS_ERR(pci->dbi_base2))
		return PTR_ERR(pci->dbi_base2);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res)
		return -EINVAL;

	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);

	ret = dw_pcie6_ep_init(ep);
	if (ret) {
		dev_err(dev, "Failed to initialize endpoint\n");
		return ret;
	}
	return 0;
}

static int dw_plat_pcie6_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_plat_pcie6 *dw_plat_pcie6;
	struct dw_pcie6 *pci;
	struct resource *res;  /* Resource from DT */
	int ret;
	const struct of_device_id *match;
	const struct dw_plat_pcie6_of_data *data;
	enum dw_pcie6_device_mode mode;

	match = of_match_device(dw_plat_pcie6_of_match, dev);
	if (!match)
		return -EINVAL;

	data = (struct dw_plat_pcie6_of_data *)match->data;
	mode = (enum dw_pcie6_device_mode)data->mode;

	dw_plat_pcie6 = devm_kzalloc(dev, sizeof(*dw_plat_pcie6), GFP_KERNEL);
	if (!dw_plat_pcie6)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie6_ops;

	dw_plat_pcie6->pci = pci;
	dw_plat_pcie6->mode = mode;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	if (!res)
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	pci->dbi_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pci->dbi_base))
		return PTR_ERR(pci->dbi_base);

	platform_set_drvdata(pdev, dw_plat_pcie6);

	switch (dw_plat_pcie6->mode) {
	case DW_PCIE_RC_TYPE:
		if (!IS_ENABLED(CONFIG_PCIE6_DW_HOST))
			return -ENODEV;

		ret = dw_plat_add_pcie_port(dw_plat_pcie6, pdev);
		if (ret < 0)
			return ret;
		break;
	case DW_PCIE_EP_TYPE:
		if (!IS_ENABLED(CONFIG_PCIE6_DW_EP))
			return -ENODEV;

		ret = dw_plat_add_pcie_ep(dw_plat_pcie6, pdev);
		if (ret < 0)
			return ret;
		break;
	default:
		dev_err(dev, "INVALID device type %d\n", dw_plat_pcie6->mode);
	}

	return 0;
}

static const struct dw_plat_pcie6_of_data dw_plat_pcie6_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
};

static const struct dw_plat_pcie6_of_data dw_plat_pcie6_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
};

static const struct of_device_id dw_plat_pcie6_of_match[] = {
	{
		.compatible = "renesas,rcar-gen5-pcie6",
		.data = &dw_plat_pcie6_rc_of_data,
	},
	{
		.compatible = "renesas,rcar-gen5-pcie6-ep",
		.data = &dw_plat_pcie6_ep_of_data,
	},
	{},
};

static struct platform_driver dw_plat_pcie6_driver = {
	.driver = {
		.name	= "pcie6-rcar-gen5",
		.of_match_table = dw_plat_pcie6_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = dw_plat_pcie6_probe,
};
builtin_platform_driver(dw_plat_pcie6_driver);
